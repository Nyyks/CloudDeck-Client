package agent

import (
	"bufio"
	"io"
	"net"
	"os"
	"strings"
	"sync"
	"syscall"
	"testing"
	"time"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/usbip"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/vhci"
)

const fp = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

func TestParseHeader(t *testing.T) {
	r, err := ParseHeader("CDC1 " + strings.ToUpper(fp) + " /clouddeck/v1/usb/attach?busid=1-2\n")
	if err != nil || r.Client != fp || r.Path != "/clouddeck/v1/usb/attach" || r.Query.Get("busid") != "1-2" {
		t.Fatalf("%+v %v", r, err)
	}
	for _, bad := range []string{
		"",
		"CDC2 " + fp + " /clouddeck/v1/hello",
		"CDC1 abc /clouddeck/v1/hello",
		"CDC1 " + strings.Repeat("z", 64) + " /clouddeck/v1/hello",
		"CDC1 " + fp + " /etc/passwd",
		"CDC1 " + fp + " /clouddeck/v1/hello extra",
	} {
		if _, err := ParseHeader(bad); err == nil {
			t.Errorf("accepted %q", bad)
		}
	}
}

// fakeVHCI stands in for the kernel: Attach dups the socket fd it is given, and
// the test then talks USB/IP "URBs" through it.
type fakeVHCI struct {
	mu       sync.Mutex
	ports    []vhci.Port
	attached chan net.Conn
	detached chan int
	lastArgs []uint32
}

func newFake() *fakeVHCI {
	f := &fakeVHCI{attached: make(chan net.Conn, 1), detached: make(chan int, 1)}
	for i := 0; i < 8; i++ {
		f.ports = append(f.ports, vhci.Port{Hub: "hs", Port: i, Status: vhci.StatusNull})
	}
	for i := 8; i < 16; i++ {
		f.ports = append(f.ports, vhci.Port{Hub: "ss", Port: i, Status: vhci.StatusNull})
	}
	f.ports[0].Status = vhci.StatusUsed
	return f
}

func (f *fakeVHCI) Ports() ([]vhci.Port, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]vhci.Port(nil), f.ports...), nil
}

func (f *fakeVHCI) Attach(port int, fd uintptr, devid, speed uint32) error {
	dup, err := syscall.Dup(int(fd))
	if err != nil {
		return err
	}
	c, err := net.FileConn(os.NewFile(uintptr(dup), "kernel"))
	if err != nil {
		return err
	}
	f.mu.Lock()
	f.ports[port].Status = vhci.StatusUsed
	f.lastArgs = []uint32{uint32(port), devid, speed}
	f.mu.Unlock()
	f.attached <- c
	return nil
}

func (f *fakeVHCI) Detach(port int) error {
	f.mu.Lock()
	f.ports[port].Status = vhci.StatusNull
	f.mu.Unlock()
	f.detached <- port
	return nil
}

// bufConn reads through a bufio.Reader that may already hold bytes from the conn.
type bufConn struct {
	r *bufio.Reader
	net.Conn
}

func (b bufConn) Read(p []byte) (int, error) { return b.r.Read(p) }

// exporter plays the client side (usbipd-win etc.): answers the import, then echoes.
func exporter(t *testing.T, c net.Conn, dev usbip.Device) {
	busid, err := usbip.ReadImportRequest(c)
	if err != nil || busid != dev.BusID {
		t.Errorf("exporter got busid %q err %v", busid, err)
		return
	}
	if err := usbip.WriteImportReply(c, usbip.StatusOK, dev); err != nil {
		t.Errorf("reply: %v", err)
		return
	}
	io.Copy(c, c) // echo "URBs" back
}

func TestAttachRelayAndDetach(t *testing.T) {
	fake := newFake()
	s := &Server{VHCI: fake, Version: "test"}
	l, err := net.Listen("unix", t.TempDir()+"/agent.sock")
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()
	go s.Serve(l)

	// Sunshine's side of the tunnel
	c, err := net.Dial("unix", l.Addr().String())
	if err != nil {
		t.Fatal(err)
	}
	io.WriteString(c, "CDC1 "+fp+" /clouddeck/v1/usb/attach?busid=1-2\n")
	br := bufio.NewReader(c)
	if line, _ := br.ReadString('\n'); line != "OK\n" {
		t.Fatalf("header reply %q", line)
	}
	dev := usbip.Device{BusID: "1-2", BusNum: 1, DevNum: 5, Speed: usbip.SpeedHigh, IDVendor: 0x045e, IDProduct: 0x028e}
	go exporter(t, bufConn{br, c}, dev)

	var kernel net.Conn
	select {
	case kernel = <-fake.attached:
	case <-time.After(3 * time.Second):
		t.Fatal("no attach")
	}
	if got := fake.lastArgs; got[0] != 1 || got[1] != 1<<16|5 || got[2] != usbip.SpeedHigh {
		t.Fatalf("attach args port/devid/speed = %v (port 0 was busy)", got)
	}
	if l := s.List(fp); len(l) != 1 || l[0].BusID != "1-2" || l[0].VendorID != "045e" {
		t.Fatalf("list = %+v", l)
	}
	if l := s.List(strings.Repeat("f", 64)); len(l) != 0 {
		t.Fatalf("other client sees %+v", l)
	}

	// "kernel" -> tunnel -> exporter (echo) -> tunnel -> "kernel"
	kernel.Write([]byte("CMD_SUBMIT"))
	buf := make([]byte, 10)
	kernel.SetReadDeadline(time.Now().Add(3 * time.Second))
	if _, err := io.ReadFull(kernel, buf); err != nil || string(buf) != "CMD_SUBMIT" {
		t.Fatalf("relay: %q %v", buf, err)
	}

	// Client disconnects -> agent detaches the port.
	c.Close()
	select {
	case p := <-fake.detached:
		if p != 1 {
			t.Fatalf("detached port %d", p)
		}
	case <-time.After(3 * time.Second):
		t.Fatal("no detach after tunnel close")
	}
	time.Sleep(50 * time.Millisecond)
	if l := s.List(fp); len(l) != 0 {
		t.Fatalf("still listed after detach: %+v", l)
	}
}

func TestSuperSpeedUsesSSPort(t *testing.T) {
	fake := newFake()
	s := &Server{VHCI: fake}
	a, b := net.Pipe()
	dev := usbip.Device{BusID: "2-1", BusNum: 2, DevNum: 3, Speed: usbip.SpeedSuper}
	go exporter(t, b, dev)
	go s.Attach(fp, "2-1", a, a)
	select {
	case k := <-fake.attached:
		defer k.Close()
		if fake.lastArgs[0] != 8 {
			t.Fatalf("superspeed device on port %d, want first ss port 8", fake.lastArgs[0])
		}
	case <-time.After(3 * time.Second):
		t.Fatal("no attach")
	}
	a.Close()
}

func TestRefusedImportDoesNotAttach(t *testing.T) {
	fake := newFake()
	s := &Server{VHCI: fake}
	a, b := net.Pipe()
	go func() {
		usbip.ReadImportRequest(b)
		usbip.WriteImportReply(b, 1, usbip.Device{})
		b.Close()
	}()
	if err := s.Attach(fp, "1-9", a, a); err == nil {
		t.Fatal("refused import should error")
	}
	select {
	case <-fake.attached:
		t.Fatal("attached despite refusal")
	default:
	}
}

func TestHelloAndUnknown(t *testing.T) {
	s := &Server{VHCI: newFake(), Version: "1.2.3"}
	l, _ := net.Listen("unix", t.TempDir()+"/a.sock")
	defer l.Close()
	go s.Serve(l)
	ask := func(line string) string {
		c, err := net.Dial("unix", l.Addr().String())
		if err != nil {
			t.Fatal(err)
		}
		defer c.Close()
		io.WriteString(c, line)
		r, _ := bufio.NewReader(c).ReadString('\n')
		return r
	}
	if r := ask("CDC1 " + fp + " /clouddeck/v1/hello\n"); !strings.HasPrefix(r, `OK {"agent":"clouddeck-agent","features":["usb"],"version":"1.2.3"}`) {
		t.Fatalf("hello = %q", r)
	}
	if r := ask("CDC1 " + fp + " /clouddeck/v1/nope\n"); r != "ERR unknown operation\n" {
		t.Fatalf("unknown = %q", r)
	}
	if r := ask("garbage\n"); !strings.HasPrefix(r, "ERR ") {
		t.Fatalf("garbage = %q", r)
	}
	if r := ask("CDC1 " + fp + " /clouddeck/v1/usb/attach?busid=1-2;x\n"); r != "ERR invalid busid\n" {
		t.Fatalf("bad busid = %q", r)
	}
}
