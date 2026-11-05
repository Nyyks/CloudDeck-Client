package agent

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/display"
)

// tinyKWin: one output with a 1080p and a 3840x1080 mode.
type tinyKWin struct {
	mu      sync.Mutex
	current string
}

func (k *tinyKWin) run(args ...string) ([]byte, error) {
	k.mu.Lock()
	defer k.mu.Unlock()
	if args[0] == "-j" {
		return []byte(fmt.Sprintf(`{"outputs":[{"name":"HDMI-A-1","enabled":true,"connected":true,"priority":1,"currentModeId":%q,
			"modes":[{"id":"1","size":{"width":1920,"height":1080},"refreshRate":60},
			         {"id":"2","size":{"width":3840,"height":1080},"refreshRate":60}]}]}`, k.current)), nil
	}
	if strings.HasPrefix(args[0], "output.HDMI-A-1.mode.") {
		k.current = strings.TrimPrefix(args[0], "output.HDMI-A-1.mode.")
		return nil, nil
	}
	return nil, fmt.Errorf("exit status 9")
}

func (k *tinyKWin) mode() string {
	k.mu.Lock()
	defer k.mu.Unlock()
	return k.current
}

func TestDisplaySetHoldsUntilTunnelCloses(t *testing.T) {
	kw := &tinyKWin{current: "1"}
	s := &Server{VHCI: newFake(), Display: &display.Manager{Run: kw.run}, Version: "t"}
	l, _ := net.Listen("unix", t.TempDir()+"/a.sock")
	defer l.Close()
	go s.Serve(l)

	c, err := net.Dial("unix", l.Addr().String())
	if err != nil {
		t.Fatal(err)
	}
	io.WriteString(c, "CDC1 "+fp+" /clouddeck/v1/display/set?width=3840&height=1080&fps=60\n")
	br := bufio.NewReader(c)
	if line, _ := br.ReadString('\n'); line != "OK\n" {
		t.Fatalf("reply = %q", line)
	}
	statusLine, _ := br.ReadString('\n')
	var st map[string]any
	if err := json.Unmarshal([]byte(statusLine), &st); err != nil || st["width"] != float64(3840) || st["restores"] != "1920x1080@60.00" {
		t.Fatalf("status = %q (%v)", statusLine, err)
	}
	if kw.mode() != "2" {
		t.Fatalf("mode = %s", kw.mode())
	}
	io.WriteString(c, "\n\n") // keepalives are ignored
	time.Sleep(50 * time.Millisecond)
	if kw.mode() != "2" {
		t.Fatal("restored while the tunnel was open")
	}
	c.Close()
	deadline := time.Now().Add(2 * time.Second)
	for kw.mode() != "1" {
		if time.Now().After(deadline) {
			t.Fatal("not restored after the tunnel closed")
		}
		time.Sleep(10 * time.Millisecond)
	}
	s.Wait()
}

func TestDisplaySetErrors(t *testing.T) {
	ask := func(s *Server, line string) string {
		a, b := net.Pipe()
		go s.handle(b)
		defer a.Close()
		io.WriteString(a, line)
		r, _ := bufio.NewReader(a).ReadString('\n')
		return r
	}
	none := &Server{VHCI: newFake()}
	if r := ask(none, "CDC1 "+fp+" /clouddeck/v1/display/set?width=3840&height=1080&fps=60\n"); !strings.HasPrefix(r, "ERR display matching is not configured") {
		t.Fatalf("no display = %q", r)
	}
	kw := &tinyKWin{current: "1"}
	s := &Server{VHCI: newFake(), Display: &display.Manager{Run: kw.run}}
	if r := ask(s, "CDC1 "+fp+" /clouddeck/v1/display/set?width=abc&height=1080&fps=60\n"); !strings.HasPrefix(r, "ERR size") {
		t.Fatalf("bad width = %q", r)
	}
	if r := ask(s, "CDC1 "+fp+" /clouddeck/v1/hello\n"); !strings.Contains(r, `"features":["usb","display"]`) {
		t.Fatalf("hello = %q", r)
	}
	s.Wait()
	if kw.mode() != "1" {
		t.Fatal("mode changed by a failed request")
	}
}

func TestDetachAllRestoresDisplay(t *testing.T) {
	kw := &tinyKWin{current: "1"}
	s := &Server{VHCI: newFake(), Display: &display.Manager{Run: kw.run}}
	a, b := net.Pipe()
	go s.handle(b)
	defer a.Close()
	io.WriteString(a, "CDC1 "+fp+" /clouddeck/v1/display/set?width=3840&height=1080&fps=60\n")
	br := bufio.NewReader(a)
	br.ReadString('\n')
	br.ReadString('\n')
	go io.Copy(io.Discard, br)
	s.DetachAll()
	s.Wait()
	if kw.mode() != "1" {
		t.Fatalf("mode = %s after shutdown", kw.mode())
	}
}

type memClipboard struct {
	mu   sync.Mutex
	text string
}

func (m *memClipboard) Get() (string, error) { m.mu.Lock(); defer m.mu.Unlock(); return m.text, nil }
func (m *memClipboard) Set(t string) error   { m.mu.Lock(); defer m.mu.Unlock(); m.text = t; return nil }

func TestClipboardSyncOverServer(t *testing.T) {
	cb := &memClipboard{text: "vm"}
	s := &Server{VHCI: newFake(), Clipboard: cb, ClipboardPoll: 10 * time.Millisecond}
	a, b := net.Pipe()
	go s.handle(b)
	io.WriteString(a, "CDC1 "+fp+" /clouddeck/v1/clipboard/sync\n")
	br := bufio.NewReader(a)
	if line, _ := br.ReadString('\n'); line != "OK\n" {
		t.Fatalf("reply = %q", line)
	}
	if line, _ := br.ReadString('\n'); !strings.Contains(line, `"max_bytes":102400`) {
		t.Fatalf("status = %q", line)
	}
	io.WriteString(a, `{"text":"from client"}`+"\n")
	deadline := time.Now().Add(2 * time.Second)
	for {
		if txt, _ := cb.Get(); txt == "from client" {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("client text not applied")
		}
		time.Sleep(5 * time.Millisecond)
	}
	cb.Set("copied on vm")
	if line, _ := br.ReadString('\n'); line != `{"text":"copied on vm"}`+"\n" {
		t.Fatalf("host update = %q", line)
	}
	go io.Copy(io.Discard, br)
	s.DetachAll()
	s.Wait()

	none := &Server{VHCI: newFake()}
	x, y := net.Pipe()
	go none.handle(y)
	io.WriteString(x, "CDC1 "+fp+" /clouddeck/v1/clipboard/sync\n")
	if line, _ := bufio.NewReader(x).ReadString('\n'); !strings.HasPrefix(line, "ERR clipboard sync is not configured") {
		t.Fatalf("no clipboard = %q", line)
	}
}

type memUploads struct {
	mu   sync.Mutex
	name string
	data []byte
}

func (m *memUploads) Save(_ context.Context, name string, size int64, r io.Reader) (string, error) {
	b, err := io.ReadAll(io.LimitReader(r, size))
	if err != nil {
		return "", err
	}
	if int64(len(b)) < size {
		return "", fmt.Errorf("short: %d of %d", len(b), size)
	}
	m.mu.Lock()
	m.name, m.data = name, b
	m.mu.Unlock()
	return "/home/user/Downloads/CloudDeck/" + name, nil
}

func TestFileUploadOverServer(t *testing.T) {
	up := &memUploads{}
	s := &Server{VHCI: newFake(), Uploads: up}
	a, b := net.Pipe()
	go s.handle(b)
	io.WriteString(a, "CDC1 "+fp+" /clouddeck/v1/file/upload?name=..%2F..%2Fevil.txt&size=5\n")
	br := bufio.NewReader(a)
	if line, _ := br.ReadString('\n'); line != "OK\n" {
		t.Fatalf("reply = %q", line)
	}
	io.WriteString(a, "hello")
	line, _ := br.ReadString('\n')
	if line != `{"saved":"/home/user/Downloads/CloudDeck/evil.txt"}`+"\n" || up.name != "evil.txt" || string(up.data) != "hello" {
		t.Fatalf("result = %q (name %q, data %q)", line, up.name, up.data)
	}
	a.Close()
	s.Wait()

	for _, q := range []string{"name=&size=1", "name=a&size=-1", "name=a&size=x"} {
		x, y := net.Pipe()
		go s.handle(y)
		io.WriteString(x, "CDC1 "+fp+" /clouddeck/v1/file/upload?"+q+"\n")
		if l, _ := bufio.NewReader(x).ReadString('\n'); !strings.HasPrefix(l, "ERR ") {
			t.Fatalf("%s accepted: %q", q, l)
		}
		x.Close()
	}
}
