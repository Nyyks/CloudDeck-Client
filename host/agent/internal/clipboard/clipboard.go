// Package clipboard syncs the text clipboard between a client and the VM's Plasma
// session for the length of a stream (Phase 5, docs/adr/0008-clipboard-sync.md).
//
// The VM side goes through Klipper (Plasma's clipboard service) on the session bus,
// using busctl run as the desktop user. Nothing is installed and no Wayland focus
// is needed. The tunnel carries one JSON object per line in both directions:
//
//	{"text":"..."}
//
// The client sends its clipboard when the stream starts and whenever it changes.
// The agent polls Klipper and sends host-side changes.
package clipboard

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"sync"
	"time"
)

// MaxBytes caps one clipboard text (UTF-8). busctl passes it as one argument, and
// Linux limits a single argument to 128 KiB.
const MaxBytes = 100 * 1024

// Clipboard is the VM's clipboard.
type Clipboard interface {
	Get() (string, error)
	Set(text string) error
}

// Runner runs busctl with args (as the desktop user) and returns stdout.
type Runner func(args ...string) ([]byte, error)

// Klipper talks to org.kde.klipper on the desktop user's session bus.
type Klipper struct{ Run Runner }

var klipperCall = []string{"--user", "--json=short", "--", "call", "org.kde.klipper", "/klipper", "org.kde.klipper.klipper"}

// Get returns the current clipboard text.
func (k Klipper) Get() (string, error) {
	out, err := k.Run(append(append([]string{}, klipperCall...), "getClipboardContents")...)
	if err != nil {
		return "", fmt.Errorf("klipper getClipboardContents: %w", err)
	}
	var reply struct {
		Type string   `json:"type"`
		Data []string `json:"data"`
	}
	if err := json.Unmarshal(out, &reply); err != nil || reply.Type != "s" || len(reply.Data) != 1 {
		return "", fmt.Errorf("klipper getClipboardContents: unexpected reply %q", truncate(string(out), 80))
	}
	return reply.Data[0], nil
}

// Set replaces the clipboard text.
func (k Klipper) Set(text string) error {
	// "--" ends option parsing, so a text starting with "-" stays a value.
	args := append(append([]string{}, klipperCall...), "setClipboardContents", "s", text)
	if _, err := k.Run(args...); err != nil {
		return fmt.Errorf("klipper setClipboardContents: %w", err)
	}
	return nil
}

type message struct {
	Text *string `json:"text"`
}

// Sync runs one client's sync until r reaches EOF or a write fails. It polls the
// VM clipboard every interval. The VM's clipboard at the start is not sent: the
// client's clipboard wins when a stream starts.
func Sync(cb Clipboard, r io.Reader, w io.Writer, interval time.Duration, logf func(string, ...any)) error {
	var mu sync.Mutex // serializes Get/Set with `last`, so a client update is never echoed back
	last, err := cb.Get()
	if err != nil {
		return err
	}

	done := make(chan error, 1)
	go func() {
		sc := bufio.NewScanner(r)
		sc.Buffer(make([]byte, 4096), 2*MaxBytes+1024) // JSON escaping can double the size
		for sc.Scan() {
			line := sc.Bytes()
			if len(line) == 0 {
				continue // keepalive
			}
			var m message
			if err := json.Unmarshal(line, &m); err != nil || m.Text == nil {
				logf("clipboard: ignoring malformed message")
				continue
			}
			if len(*m.Text) > MaxBytes {
				logf("clipboard: ignoring %d bytes from the client (limit %d)", len(*m.Text), MaxBytes)
				continue
			}
			mu.Lock()
			if *m.Text != last {
				if err := cb.Set(*m.Text); err != nil {
					logf("clipboard: %v", err)
				} else {
					last = *m.Text
				}
			}
			mu.Unlock()
		}
		done <- sc.Err()
	}()

	t := time.NewTicker(interval)
	defer t.Stop()
	failing := false
	for {
		select {
		case err := <-done:
			return err
		case <-t.C:
		}
		mu.Lock()
		text, err := cb.Get()
		if err != nil {
			mu.Unlock()
			if !failing {
				logf("clipboard: %v (retrying)", err)
				failing = true
			}
			continue
		}
		failing = false
		changed := text != last && len(text) <= MaxBytes
		if changed {
			last = text
		}
		mu.Unlock()
		if !changed {
			continue
		}
		b, _ := json.Marshal(message{Text: &text})
		if _, err := w.Write(append(b, '\n')); err != nil {
			return err
		}
	}
}

func truncate(s string, n int) string {
	if len(s) > n {
		return s[:n] + "..."
	}
	return s
}
