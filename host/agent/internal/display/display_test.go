package display

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

// fakeKWin mimics kscreen-doctor against one output, like the VM's HDMI-A-1.
type fakeKWin struct {
	modes      []Mode
	current    string
	nextID     int
	calls      []string
	rejectFull bool // custom modes with full blanking fail (bandwidth)
	ignoreSet  bool // KWin silently keeps the old mode
}

func newFake() *fakeKWin {
	return &fakeKWin{
		modes: []Mode{
			{"1", 1920, 1080, 60}, {"10", 2560, 1440, 143.98}, {"11", 2560, 1440, 59.95},
			{"16", 1920, 1080, 239.76}, {"18", 1920, 1080, 120},
		},
		current: "16", nextID: 100,
	}
}

func (f *fakeKWin) run(args ...string) ([]byte, error) {
	f.calls = append(f.calls, strings.Join(args, " "))
	if len(args) == 1 && args[0] == "-j" {
		type size struct {
			Width  int `json:"width"`
			Height int `json:"height"`
		}
		type mode struct {
			ID   string  `json:"id"`
			Name string  `json:"name"`
			Size size    `json:"size"`
			RR   float64 `json:"refreshRate"`
		}
		var ms []mode
		for _, m := range f.modes {
			ms = append(ms, mode{m.ID, fmt.Sprintf("%dx%d@%.0f", m.Width, m.Height, m.RefreshRate), size{m.Width, m.Height}, m.RefreshRate})
		}
		doc := map[string]any{"outputs": []any{
			map[string]any{"id": 2, "name": "DP-1", "enabled": false, "connected": false, "priority": 0, "currentModeId": "", "modes": []any{}},
			map[string]any{"id": 1, "name": "HDMI-A-1", "enabled": true, "connected": true, "priority": 1, "currentModeId": f.current, "modes": ms},
		}}
		b, _ := json.Marshal(doc)
		return append([]byte("Detected locale \"C\"...\n"), b...), nil
	}
	if len(args) != 1 {
		return nil, errors.New("unexpected args")
	}
	p := strings.Split(args[0], ".")
	if len(p) < 4 || p[0] != "output" || p[1] != "HDMI-A-1" {
		return nil, errors.New("exit status 9")
	}
	switch p[2] {
	case "mode":
		for _, m := range f.modes {
			if m.ID == p[3] {
				if !f.ignoreSet {
					f.current = m.ID
				}
				return nil, nil
			}
		}
		return nil, errors.New("exit status 9")
	case "addCustomMode":
		if len(p) != 7 {
			return nil, errors.New("exit status 9")
		}
		if f.rejectFull && p[6] == "full" {
			return nil, errors.New("exit status 1")
		}
		w, _ := strconv.Atoi(p[3])
		h, _ := strconv.Atoi(p[4])
		mhz, _ := strconv.Atoi(p[5])
		f.modes = append(f.modes, Mode{strconv.Itoa(f.nextID), w, h, float64(mhz) / 1000})
		f.nextID++
		return nil, nil
	}
	return nil, errors.New("exit status 9")
}

func TestValidate(t *testing.T) {
	for _, c := range []struct {
		w, h, fps int
		ok        bool
	}{
		{3840, 1080, 60, true}, {5120, 1440, 60, true}, {3840, 1080, 120, true}, {5760, 1080, 60, true},
		{1920, 1080, 240, true}, {7680, 2160, 60, false}, {3841, 1080, 60, false}, {100, 100, 60, false},
		{1920, 1080, 500, false}, {1920, 1080, 0, false},
	} {
		if err := Validate(c.w, c.h, c.fps); (err == nil) != c.ok {
			t.Errorf("Validate(%d,%d,%d) = %v", c.w, c.h, c.fps, err)
		}
	}
}

func TestParseAndPrimary(t *testing.T) {
	f := newFake()
	b, _ := f.run("-j")
	outs, err := ParseOutputs(b)
	if err != nil {
		t.Fatal(err)
	}
	o, err := Primary(outs)
	if err != nil || o.Name != "HDMI-A-1" {
		t.Fatalf("primary = %+v, %v", o, err)
	}
	if cur, ok := o.Current(); !ok || cur.ID != "16" {
		t.Fatalf("current = %+v", cur)
	}
	// Numeric ids (other libkscreen versions) parse too.
	outs, err = ParseOutputs([]byte(`{"outputs":[{"name":"X","enabled":true,"connected":true,"priority":1,"currentModeId":3,"modes":[{"id":3,"size":{"width":800,"height":600},"refreshRate":60}]}]}`))
	if err != nil || outs[0].CurrentModeID != "3" || outs[0].Modes[0].ID != "3" {
		t.Fatalf("numeric ids: %+v %v", outs, err)
	}
	if m, ok := Find(o, 1920, 1080, 240); !ok || m.ID != "16" {
		t.Errorf("Find 240 = %+v", m)
	}
	if _, ok := Find(o, 1920, 1080, 144); ok {
		t.Error("Find 144 matched a 120/240 Hz mode")
	}
}

func TestSetExistingModeAndRestore(t *testing.T) {
	f := newFake()
	state := filepath.Join(t.TempDir(), "display-restore.json")
	m := &Manager{Run: f.run, StateFile: state}
	l, err := m.Set(2560, 1440, 60)
	if err != nil {
		t.Fatal(err)
	}
	if f.current != "11" || l.Mode.ID != "11" {
		t.Fatalf("current = %s", f.current)
	}
	if _, err := os.Stat(state); err != nil {
		t.Fatalf("state file not written: %v", err)
	}
	if err := l.Release(); err != nil {
		t.Fatal(err)
	}
	if f.current != "16" {
		t.Fatalf("not restored: %s", f.current)
	}
	if _, err := os.Stat(state); !os.IsNotExist(err) {
		t.Fatal("state file left behind")
	}
	if err := l.Release(); err != nil { // idempotent
		t.Fatal(err)
	}
}

func TestSetAddsCustomMode(t *testing.T) {
	f := newFake()
	m := &Manager{Run: f.run}
	l, err := m.Set(3840, 1080, 60)
	if err != nil {
		t.Fatal(err)
	}
	if l.Mode.Width != 3840 || f.current != l.Mode.ID {
		t.Fatalf("mode = %+v, current %s", l.Mode, f.current)
	}
	want := "output.HDMI-A-1.addCustomMode.3840.1080.60000.full"
	if !contains(f.calls, want) {
		t.Fatalf("calls %v lack %s", f.calls, want)
	}
	l.Release()
	// The second time the custom mode is reused, not added again.
	f.calls = nil
	l, err = m.Set(3840, 1080, 60)
	if err != nil {
		t.Fatal(err)
	}
	for _, c := range f.calls {
		if strings.Contains(c, "addCustomMode") {
			t.Fatalf("custom mode added twice: %v", f.calls)
		}
	}
	l.Release()
}

func TestCustomModeWidthMustBeMultipleOf8(t *testing.T) {
	f := newFake()
	m := &Manager{Run: f.run}
	if _, err := m.Set(1070, 720, 60); err == nil || !strings.Contains(err.Error(), "divisible by 8") {
		t.Fatalf("err = %v", err)
	}
	for _, c := range f.calls {
		if strings.Contains(c, "addCustomMode") {
			t.Fatalf("tried to add an impossible mode: %v", f.calls)
		}
	}
	if f.current != "16" {
		t.Fatalf("mode changed: %s", f.current)
	}
}

func TestReducedBlankingFallback(t *testing.T) {
	f := newFake()
	f.rejectFull = true
	m := &Manager{Run: f.run}
	l, err := m.Set(5120, 1440, 60)
	if err != nil {
		t.Fatal(err)
	}
	if !contains(f.calls, "output.HDMI-A-1.addCustomMode.5120.1440.60000.reduced") {
		t.Fatalf("no reduced-blanking attempt: %v", f.calls)
	}
	l.Release()
}

func TestNestedLeasesRestoreOriginalOnce(t *testing.T) {
	f := newFake()
	m := &Manager{Run: f.run}
	a, err := m.Set(3840, 1080, 60)
	if err != nil {
		t.Fatal(err)
	}
	b, err := m.Set(2560, 1440, 60)
	if err != nil {
		t.Fatal(err)
	}
	if b.From != "1920x1080@239.76" {
		t.Fatalf("second lease would restore %s", b.From)
	}
	a.Release()
	if f.current != "11" {
		t.Fatalf("restored while a lease was still open: %s", f.current)
	}
	b.Release()
	if f.current != "16" {
		t.Fatalf("not restored to the original: %s", f.current)
	}
}

func TestSetFailsWhenKWinIgnoresMode(t *testing.T) {
	f := newFake()
	f.ignoreSet = true
	state := filepath.Join(t.TempDir(), "s.json")
	m := &Manager{Run: f.run, StateFile: state}
	if _, err := m.Set(2560, 1440, 60); err == nil {
		t.Fatal("expected an error")
	}
	if _, err := os.Stat(state); !os.IsNotExist(err) {
		t.Fatal("state file left after a failed set")
	}
}

func TestRestoreSavedAfterCrash(t *testing.T) {
	f := newFake()
	state := filepath.Join(t.TempDir(), "s.json")
	m := &Manager{Run: f.run, StateFile: state}
	if _, err := m.Set(2560, 1440, 60); err != nil {
		t.Fatal(err)
	}
	// The agent dies here; a new one starts with the same state file.
	m2 := &Manager{Run: f.run, StateFile: state}
	got, err := m2.RestoreSaved()
	if err != nil || got != "1920x1080@239.76" || f.current != "16" {
		t.Fatalf("RestoreSaved = %q, %v, current %s", got, err, f.current)
	}
	if _, err := os.Stat(state); !os.IsNotExist(err) {
		t.Fatal("state file not cleared")
	}
	if got, err := m2.RestoreSaved(); got != "" || err != nil {
		t.Fatalf("second RestoreSaved = %q, %v", got, err)
	}
}

func TestStreamBeforeLeftoverRestoreKeepsTrueOriginal(t *testing.T) {
	f := newFake()
	state := filepath.Join(t.TempDir(), "s.json")
	if _, err := (&Manager{Run: f.run, StateFile: state}).Set(3840, 1080, 60); err != nil {
		t.Fatal(err)
	}
	// Crash; after the restart a stream arrives before RestoreSaved ran.
	m := &Manager{Run: f.run, StateFile: state}
	l, err := m.Set(2560, 1440, 60)
	if err != nil {
		t.Fatal(err)
	}
	if l.From != "1920x1080@239.76" {
		t.Fatalf("would restore %s instead of the pre-crash mode", l.From)
	}
	if got, _ := m.RestoreSaved(); got != "" || f.current != "11" {
		t.Fatalf("RestoreSaved changed the mode mid-stream (%q, current %s)", got, f.current)
	}
	l.Release()
	if f.current != "16" {
		t.Fatalf("current = %s", f.current)
	}
}

func TestReleaseAll(t *testing.T) {
	f := newFake()
	m := &Manager{Run: f.run}
	if _, err := m.Set(3840, 1080, 60); err != nil {
		t.Fatal(err)
	}
	if err := m.ReleaseAll(); err != nil || f.current != "16" {
		t.Fatalf("ReleaseAll: %v, current %s", err, f.current)
	}
}

func TestSetRejectsInvalid(t *testing.T) {
	f := newFake()
	m := &Manager{Run: f.run}
	if _, err := m.Set(7680, 2160, 60); err == nil {
		t.Fatal("expected bandwidth error")
	}
	if len(f.calls) != 0 {
		t.Fatalf("kscreen-doctor called for an invalid request: %v", f.calls)
	}
}

func contains(list []string, s string) bool {
	for _, x := range list {
		if x == s {
			return true
		}
	}
	return false
}
