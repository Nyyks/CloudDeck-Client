// Package display switches the VM's desktop resolution for a stream and restores it
// afterwards (Phase 4a, docs/adr/0007-multi-monitor.md). It drives KWin through
// kscreen-doctor, run as the desktop user inside the Plasma Wayland session.
//
// A client holds a "lease" (an open tunnel) for the length of its stream. The first
// lease remembers the mode that was active before; when the last lease ends the
// agent switches back to it. The remembered mode is also written to a state file,
// so a crash or reboot mid-stream is repaired on the next agent start.
//
// Custom modes that had to be added stay in KWin's list (they are harmless, and the
// next stream at that size reuses them).
package display

import (
	"encoding/json"
	"errors"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
)

// Runner runs kscreen-doctor with args and returns its stdout.
type Runner func(args ...string) ([]byte, error)

// Limits for requested modes. The pixel rate cap keeps requests inside what the
// GPU's HDMI output and the encoder handle: it allows 5120x1440@60, 3840x1080@120
// and 5760x1080@60, and rejects 7680x2160@60.
const (
	MinWidth, MaxWidth   = 640, 7680
	MinHeight, MaxHeight = 360, 4320
	MinFPS, MaxFPS       = 24, 240
	MaxPixelRate         = 3840 * 2160 * 60
)

// Mode is one entry of an output's mode list.
type Mode struct {
	ID          string
	Width       int
	Height      int
	RefreshRate float64
}

func (m Mode) String() string { return fmt.Sprintf("%dx%d@%.2f", m.Width, m.Height, m.RefreshRate) }

// Output is the part of a kscreen-doctor output that matters here.
type Output struct {
	Name          string
	Enabled       bool
	Connected     bool
	Priority      int
	CurrentModeID string
	Modes         []Mode
}

// Current returns the active mode.
func (o Output) Current() (Mode, bool) {
	for _, m := range o.Modes {
		if m.ID == o.CurrentModeID {
			return m, true
		}
	}
	return Mode{}, false
}

// flexString accepts a JSON string or number (libkscreen versions differ).
type flexString string

func (f *flexString) UnmarshalJSON(b []byte) error {
	var s string
	if json.Unmarshal(b, &s) == nil {
		*f = flexString(s)
		return nil
	}
	var n json.Number
	if err := json.Unmarshal(b, &n); err != nil {
		return err
	}
	*f = flexString(n.String())
	return nil
}

// ParseOutputs parses `kscreen-doctor -j`.
func ParseOutputs(b []byte) ([]Output, error) {
	var doc struct {
		Outputs []struct {
			Name          string     `json:"name"`
			Enabled       *bool      `json:"enabled"`
			Connected     bool       `json:"connected"`
			Priority      int        `json:"priority"`
			CurrentModeID flexString `json:"currentModeId"`
			Modes         []struct {
				ID   flexString `json:"id"`
				Size struct {
					Width  int `json:"width"`
					Height int `json:"height"`
				} `json:"size"`
				RefreshRate float64 `json:"refreshRate"`
			} `json:"modes"`
		} `json:"outputs"`
	}
	// kscreen-doctor may print warnings before the JSON document.
	if i := strings.IndexByte(string(b), '{'); i > 0 {
		b = b[i:]
	}
	if err := json.Unmarshal(b, &doc); err != nil {
		return nil, fmt.Errorf("kscreen-doctor -j: %w", err)
	}
	out := make([]Output, 0, len(doc.Outputs))
	for _, o := range doc.Outputs {
		x := Output{
			Name: o.Name, Connected: o.Connected, Priority: o.Priority,
			Enabled: o.Enabled == nil || *o.Enabled, CurrentModeID: string(o.CurrentModeID),
		}
		for _, m := range o.Modes {
			x.Modes = append(x.Modes, Mode{ID: string(m.ID), Width: m.Size.Width, Height: m.Size.Height, RefreshRate: m.RefreshRate})
		}
		out = append(out, x)
	}
	return out, nil
}

// Primary picks the output Sunshine streams: enabled and connected, lowest
// priority number (1 = primary) first.
func Primary(outs []Output) (Output, error) {
	best := -1
	for i, o := range outs {
		if !o.Enabled || !o.Connected || len(o.Modes) == 0 {
			continue
		}
		if best < 0 || (o.Priority > 0 && (outs[best].Priority == 0 || o.Priority < outs[best].Priority)) {
			best = i
		}
	}
	if best < 0 {
		return Output{}, errors.New("no enabled display output")
	}
	return outs[best], nil
}

// Find returns the mode of exactly width x height whose refresh rate is closest to
// fps, if that is within 0.5 Hz.
func Find(o Output, width, height, fps int) (Mode, bool) {
	var best Mode
	found := false
	for _, m := range o.Modes {
		if m.Width != width || m.Height != height {
			continue
		}
		d := math.Abs(m.RefreshRate - float64(fps))
		if d <= 0.5 && (!found || d < math.Abs(best.RefreshRate-float64(fps))) {
			best, found = m, true
		}
	}
	return best, found
}

// Validate checks a requested mode.
func Validate(width, height, fps int) error {
	switch {
	case width < MinWidth || width > MaxWidth || height < MinHeight || height > MaxHeight:
		return fmt.Errorf("size %dx%d out of range", width, height)
	case width%2 != 0 || height%2 != 0:
		return fmt.Errorf("size %dx%d must be even", width, height)
	case fps < MinFPS || fps > MaxFPS:
		return fmt.Errorf("refresh rate %d out of range", fps)
	case width*height*fps > MaxPixelRate:
		return fmt.Errorf("%dx%d@%d exceeds the display bandwidth limit", width, height, fps)
	}
	return nil
}

// saved is the state file content.
type saved struct {
	Output string `json:"output"`
	ModeID string `json:"mode_id"`
	Mode   string `json:"mode"`
}

// Manager serializes mode changes and tracks leases.
type Manager struct {
	Run       Runner
	StateFile string // remembers the mode to restore; empty disables persistence

	mu       sync.Mutex
	leases   int
	original *saved
}

// Lease is held for the length of a stream.
type Lease struct {
	m    *Manager
	once sync.Once
	Mode Mode   // the mode now active
	From string // the mode that will be restored
}

func (m *Manager) outputs() ([]Output, error) {
	b, err := m.Run("-j")
	if err != nil {
		return nil, fmt.Errorf("kscreen-doctor -j: %w", err)
	}
	return ParseOutputs(b)
}

// Set switches the primary output to width x height @ fps (adding a custom mode if
// needed) and returns a lease. Release the lease to switch back.
func (m *Manager) Set(width, height, fps int) (*Lease, error) {
	if err := Validate(width, height, fps); err != nil {
		return nil, err
	}
	m.mu.Lock()
	defer m.mu.Unlock()

	outs, err := m.outputs()
	if err != nil {
		return nil, err
	}
	o, err := Primary(outs)
	if err != nil {
		return nil, err
	}
	cur, ok := o.Current()
	if !ok {
		return nil, fmt.Errorf("%s: current mode %q not in its mode list", o.Name, o.CurrentModeID)
	}
	adopted := false
	if m.leases == 0 {
		if left, ok := m.loadState(); ok && left.Output == o.Name {
			// A crash left the display changed and RestoreSaved hasn't run yet:
			// the mode to go back to is the one remembered then, not the current one.
			m.original, adopted = &left, true
		} else {
			m.original = &saved{Output: o.Name, ModeID: cur.ID, Mode: cur.String()}
			if err := m.persist(); err != nil {
				return nil, err
			}
		}
	}

	target, ok := Find(o, width, height, fps)
	if !ok {
		target, err = m.addCustomMode(o.Name, width, height, fps)
		if err != nil {
			m.dropOriginalIfUnused(adopted)
			return nil, err
		}
	}
	if target.ID != cur.ID {
		if err := m.apply(o.Name, target.ID); err != nil {
			m.dropOriginalIfUnused(adopted)
			return nil, err
		}
	}
	m.leases++
	return &Lease{m: m, Mode: target, From: m.original.Mode}, nil
}

func (m *Manager) addCustomMode(output string, width, height, fps int) (Mode, error) {
	if width%8 != 0 {
		// KWin generates custom modes with CVT timings, which round the width down
		// to a multiple of 8; the requested mode would never appear.
		return Mode{}, fmt.Errorf("%dx%d is not available and custom modes need a width divisible by 8", width, height)
	}
	mhz := strconv.Itoa(fps * 1000)
	var lastErr error
	// Full blanking first (what a monitor would advertise); reduced blanking needs
	// less bandwidth and is the fallback for very wide modes.
	for _, blanking := range []string{"full", "reduced"} {
		arg := fmt.Sprintf("output.%s.addCustomMode.%d.%d.%s.%s", output, width, height, mhz, blanking)
		if _, err := m.Run(arg); err != nil {
			lastErr = err
			continue
		}
		outs, err := m.outputs()
		if err != nil {
			return Mode{}, err
		}
		for _, o := range outs {
			if o.Name == output {
				if mode, ok := Find(o, width, height, fps); ok {
					return mode, nil
				}
			}
		}
		lastErr = errors.New("mode did not appear")
	}
	return Mode{}, fmt.Errorf("adding custom mode %dx%d@%d on %s: %w", width, height, fps, output, lastErr)
}

// apply selects modeID on output and checks that KWin really switched.
func (m *Manager) apply(output, modeID string) error {
	if _, err := m.Run(fmt.Sprintf("output.%s.mode.%s", output, modeID)); err != nil {
		return fmt.Errorf("setting mode %s on %s: %w", modeID, output, err)
	}
	outs, err := m.outputs()
	if err != nil {
		return err
	}
	for _, o := range outs {
		if o.Name == output {
			if o.CurrentModeID != modeID {
				return fmt.Errorf("%s stayed on mode %s instead of %s", output, o.CurrentModeID, modeID)
			}
			return nil
		}
	}
	return fmt.Errorf("output %s disappeared", output)
}

// Release ends the lease; the last one restores the original mode.
func (l *Lease) Release() error {
	var err error
	l.once.Do(func() { err = l.m.release() })
	return err
}

func (m *Manager) release() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.leases > 0 {
		m.leases--
	}
	if m.leases > 0 || m.original == nil {
		return nil
	}
	err := m.restoreLocked(*m.original)
	if err == nil {
		m.original = nil
		m.clearState()
	}
	return err
}

func (m *Manager) restoreLocked(s saved) error {
	outs, err := m.outputs()
	if err != nil {
		return err
	}
	for _, o := range outs {
		if o.Name != s.Output {
			continue
		}
		if o.CurrentModeID == s.ModeID {
			return nil
		}
		return m.apply(s.Output, s.ModeID)
	}
	return fmt.Errorf("output %s not found", s.Output)
}

// RestoreSaved repairs a mode left behind by a crash or reboot mid-stream. It is
// called at agent start, before any lease exists. A missing state file is fine.
func (m *Manager) RestoreSaved() (restored string, err error) {
	if m.StateFile == "" {
		return "", nil
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.leases > 0 {
		return "", nil // a stream adopted the leftover; its lease restores it
	}
	if _, err := os.Stat(m.StateFile); errors.Is(err, os.ErrNotExist) {
		return "", nil
	}
	s, ok := m.loadState()
	if !ok {
		m.clearState()
		return "", fmt.Errorf("ignoring unreadable %s", m.StateFile)
	}
	if err := m.restoreLocked(s); err != nil {
		return "", err // keep the file and retry on the next start
	}
	m.clearState()
	return s.Mode, nil
}

// ReleaseAll restores the original mode regardless of open leases (agent shutdown).
func (m *Manager) ReleaseAll() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.original == nil {
		return nil
	}
	err := m.restoreLocked(*m.original)
	if err == nil {
		m.original = nil
		m.leases = 0
		m.clearState()
	}
	return err
}

// dropOriginalIfUnused forgets the remembered mode after a failed first Set. An
// adopted leftover keeps its state file, so RestoreSaved still repairs it.
func (m *Manager) dropOriginalIfUnused(adopted bool) {
	if m.leases == 0 {
		m.original = nil
		if !adopted {
			m.clearState()
		}
	}
}

func (m *Manager) persist() error {
	if m.StateFile == "" {
		return nil
	}
	b, _ := json.Marshal(m.original)
	if err := os.MkdirAll(filepath.Dir(m.StateFile), 0o755); err != nil {
		return err
	}
	tmp := m.StateFile + ".tmp"
	if err := os.WriteFile(tmp, append(b, '\n'), 0o644); err != nil {
		return err
	}
	return os.Rename(tmp, m.StateFile)
}

func (m *Manager) loadState() (saved, bool) {
	var s saved
	if m.StateFile == "" {
		return s, false
	}
	b, err := os.ReadFile(m.StateFile)
	if err != nil || json.Unmarshal(b, &s) != nil || s.Output == "" || s.ModeID == "" {
		return saved{}, false
	}
	return s, true
}

func (m *Manager) clearState() {
	if m.StateFile != "" {
		_ = os.Remove(m.StateFile)
	}
}
