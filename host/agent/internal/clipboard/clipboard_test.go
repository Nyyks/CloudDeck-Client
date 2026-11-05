package clipboard

import (
	"bufio"
	"encoding/json"
	"errors"
	"io"
	"net"
	"strings"
	"sync"
	"testing"
	"time"
)

type fakeCB struct {
	mu   sync.Mutex
	text string
	sets int
}

func (f *fakeCB) Get() (string, error) { f.mu.Lock(); defer f.mu.Unlock(); return f.text, nil }
func (f *fakeCB) Set(t string) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.text = t
	f.sets++
	return nil
}
func (f *fakeCB) host(t string) { f.mu.Lock(); defer f.mu.Unlock(); f.text = t }
func (f *fakeCB) get() string   { f.mu.Lock(); defer f.mu.Unlock(); return f.text }

func send(t *testing.T, c net.Conn, text string) {
	b, _ := json.Marshal(map[string]string{"text": text})
	if _, err := c.Write(append(b, '\n')); err != nil {
		t.Fatal(err)
	}
}

func recv(t *testing.T, br *bufio.Reader, c net.Conn) string {
	c.SetReadDeadline(time.Now().Add(2 * time.Second))
	line, err := br.ReadString('\n')
	if err != nil {
		t.Fatalf("no message: %v", err)
	}
	var m struct{ Text string }
	json.Unmarshal([]byte(line), &m)
	return m.Text
}

func waitFor(t *testing.T, cond func() bool) {
	deadline := time.Now().Add(2 * time.Second)
	for !cond() {
		if time.Now().After(deadline) {
			t.Fatal("timed out")
		}
		time.Sleep(5 * time.Millisecond)
	}
}

func TestSyncBothWaysWithoutEcho(t *testing.T) {
	cb := &fakeCB{text: "host start"}
	client, agent := net.Pipe()
	done := make(chan error, 1)
	go func() { done <- Sync(cb, agent, agent, 10*time.Millisecond, t.Logf) }()
	br := bufio.NewReader(client)

	// Client -> VM, including newlines, unicode and a leading dash.
	text := "-n line1\nline2 ✓"
	send(t, client, text)
	waitFor(t, func() bool { return cb.get() == text })

	// VM -> client.
	cb.host("copied on the VM")
	if got := recv(t, br, client); got != "copied on the VM" {
		t.Fatalf("got %q", got)
	}

	// The client's own update must not come back.
	send(t, client, "from client")
	waitFor(t, func() bool { return cb.get() == "from client" })
	client.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
	if line, err := br.ReadString('\n'); err == nil {
		t.Fatalf("echoed: %q", line)
	}

	client.Close()
	if err := <-done; err != nil && !errors.Is(err, io.ErrClosedPipe) {
		t.Logf("Sync returned %v", err)
	}
}

func TestInitialHostClipboardNotSentAndLimits(t *testing.T) {
	cb := &fakeCB{text: "secret already on the VM"}
	client, agent := net.Pipe()
	go Sync(cb, agent, agent, 10*time.Millisecond, t.Logf)
	br := bufio.NewReader(client)

	client.SetReadDeadline(time.Now().Add(80 * time.Millisecond))
	if line, err := br.ReadString('\n'); err == nil {
		t.Fatalf("initial VM clipboard sent: %q", line)
	}
	client.SetReadDeadline(time.Time{})

	send(t, client, strings.Repeat("x", MaxBytes+1)) // ignored
	client.Write([]byte("not json\n\n"))             // ignored
	send(t, client, "ok")
	waitFor(t, func() bool { return cb.get() == "ok" })
	if cb.sets != 1 {
		t.Fatalf("sets = %d", cb.sets)
	}
	client.Close()
}

func TestKlipperArgsAndParsing(t *testing.T) {
	var got [][]string
	k := Klipper{Run: func(args ...string) ([]byte, error) {
		got = append(got, args)
		if args[len(args)-1] == "getClipboardContents" {
			return []byte(`{"type":"s","data":["hello\nworld"]}`), nil
		}
		return nil, nil
	}}
	text, err := k.Get()
	if err != nil || text != "hello\nworld" {
		t.Fatalf("Get = %q, %v", text, err)
	}
	if err := k.Set("--help"); err != nil {
		t.Fatal(err)
	}
	set := strings.Join(got[1], " ")
	if set != "--user --json=short -- call org.kde.klipper /klipper org.kde.klipper.klipper setClipboardContents s --help" {
		t.Fatalf("set args: %s", set)
	}
	bad := Klipper{Run: func(...string) ([]byte, error) { return []byte(`{"type":"i","data":[1]}`), nil }}
	if _, err := bad.Get(); err == nil {
		t.Fatal("accepted a non-string reply")
	}
}
