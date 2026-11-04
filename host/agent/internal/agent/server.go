// Package agent serves requests that Sunshine's /clouddeck/v1/* tunnel (patch 0002)
// forwards over a Unix socket. See docs/adr/0006-usb-passthrough.md.
//
// Each connection starts with one header line written by Sunshine:
//
//	CDC1 <sha256 of the verified client certificate, 64 hex> <path[?query]>\n
//
// The agent answers "OK[ <json>]\n" or "ERR <message>\n". For streaming operations
// (usb/attach) the connection then carries the tunnel's raw bytes.
package agent

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"net/url"
	"os"
	"strings"
	"sync"
	"syscall"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/usbip"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/vhci"
)

// Controller is the part of vhci.Controller the server needs (faked in tests).
type Controller interface {
	Ports() ([]vhci.Port, error)
	Attach(port int, sockfd uintptr, devid uint32, speed uint32) error
	Detach(port int) error
}

// Request is a parsed header line.
type Request struct {
	Client string // certificate fingerprint
	Path   string
	Query  url.Values
}

const maxHeader = 1024

// ParseHeader parses "CDC1 <fingerprint> <target>".
func ParseHeader(line string) (Request, error) {
	f := strings.Fields(strings.TrimRight(line, "\r\n"))
	if len(f) != 3 || f[0] != "CDC1" {
		return Request{}, errors.New("bad header")
	}
	if len(f[1]) != 64 || strings.Trim(strings.ToLower(f[1]), "0123456789abcdef") != "" {
		return Request{}, errors.New("bad client fingerprint")
	}
	u, err := url.ParseRequestURI(f[2])
	if err != nil || !strings.HasPrefix(u.Path, "/clouddeck/v1/") {
		return Request{}, errors.New("bad path")
	}
	return Request{Client: strings.ToLower(f[1]), Path: u.Path, Query: u.Query()}, nil
}

// Attachment is a device currently attached for a client.
type Attachment struct {
	BusID     string `json:"busid"`
	Port      int    `json:"port"`
	VendorID  string `json:"vendor_id"`
	ProductID string `json:"product_id"`
	Speed     uint32 `json:"speed"`
	client    string
	conn      net.Conn
}

// Server handles agent requests.
type Server struct {
	VHCI    Controller
	Version string
	Log     *log.Logger

	mu       sync.Mutex // guards attached and serializes port selection + attach
	attached []*Attachment
	active   sync.WaitGroup // running attachments (Wait blocks until all detached)
}

// Wait blocks until every attachment has been detached.
func (s *Server) Wait() { s.active.Wait() }

func (s *Server) logf(format string, a ...any) {
	if s.Log != nil {
		s.Log.Printf(format, a...)
	}
}

// Serve accepts connections until the listener is closed.
func (s *Server) Serve(l net.Listener) error {
	for {
		c, err := l.Accept()
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				return nil
			}
			return err
		}
		go s.handle(c)
	}
}

func (s *Server) handle(c net.Conn) {
	br := bufio.NewReaderSize(c, maxHeader)
	line, err := br.ReadString('\n')
	if err != nil {
		c.Close()
		return
	}
	req, err := ParseHeader(line)
	if err != nil {
		fmt.Fprintf(c, "ERR %v\n", err)
		c.Close()
		return
	}
	switch req.Path {
	case "/clouddeck/v1/hello":
		s.reply(c, map[string]any{"agent": "clouddeck-agent", "version": s.Version, "features": []string{"usb"}})
		c.Close()
	case "/clouddeck/v1/usb/list":
		s.reply(c, map[string]any{"attached": s.List(req.Client)})
		c.Close()
	case "/clouddeck/v1/usb/attach":
		busid := req.Query.Get("busid")
		if !usbip.ValidBusID(busid) {
			fmt.Fprintf(c, "ERR invalid busid\n")
			c.Close()
			return
		}
		// Accept the tunnel, then run the USB/IP import through it.
		if _, err := io.WriteString(c, "OK\n"); err != nil {
			c.Close()
			return
		}
		if err := s.Attach(req.Client, busid, c, br); err != nil {
			s.logf("usb attach %s for client %s: %v", busid, short(req.Client), err)
		}
	default:
		fmt.Fprintf(c, "ERR unknown operation\n")
		c.Close()
	}
}

func (s *Server) reply(c net.Conn, v any) {
	b, _ := json.Marshal(v)
	fmt.Fprintf(c, "OK %s\n", b)
}

// List returns the devices attached for one client.
func (s *Server) List(client string) []Attachment {
	s.mu.Lock()
	defer s.mu.Unlock()
	out := []Attachment{}
	for _, a := range s.attached {
		if a.client == client {
			out = append(out, *a)
		}
	}
	return out
}

// Attach imports busid from the exporter at the other end of tunnel (r is the
// buffered reader over tunnel), attaches it to vhci and relays until either side
// closes. It returns after the device was detached again.
func (s *Server) Attach(client, busid string, tunnel net.Conn, r io.Reader) error {
	defer tunnel.Close()
	if err := usbip.WriteImportRequest(tunnel, busid); err != nil {
		return err
	}
	dev, err := usbip.ReadImportReply(r)
	if err != nil {
		return err
	}

	fds, err := syscall.Socketpair(syscall.AF_UNIX, syscall.SOCK_STREAM|syscall.SOCK_CLOEXEC, 0)
	if err != nil {
		return err
	}
	kernelEnd := os.NewFile(uintptr(fds[0]), "vhci-kernel")
	ourEnd := os.NewFile(uintptr(fds[1]), "vhci-agent")
	local, err := net.FileConn(ourEnd)
	ourEnd.Close()
	if err != nil {
		kernelEnd.Close()
		return err
	}
	defer local.Close()

	s.mu.Lock()
	ports, err := s.VHCI.Ports()
	port := -1
	if err == nil {
		port, err = vhci.FreePort(ports, dev.IsSuperSpeed())
	}
	if err == nil {
		err = s.VHCI.Attach(port, kernelEnd.Fd(), dev.DevID(), dev.Speed)
	}
	kernelEnd.Close() // the kernel holds its own reference after a successful attach
	if err != nil {
		s.mu.Unlock()
		return fmt.Errorf("vhci attach: %w", err)
	}
	a := &Attachment{
		BusID: busid, Port: port, Speed: dev.Speed,
		VendorID: fmt.Sprintf("%04x", dev.IDVendor), ProductID: fmt.Sprintf("%04x", dev.IDProduct),
		client: client, conn: tunnel,
	}
	s.attached = append(s.attached, a)
	s.active.Add(1)
	defer s.active.Done()
	s.mu.Unlock()
	s.logf("attached %s (%04x:%04x) for client %s on vhci port %d", busid, dev.IDVendor, dev.IDProduct, short(client), port)

	// Relay until either side closes, then detach.
	done := make(chan struct{}, 2)
	go func() { io.Copy(local, r); done <- struct{}{} }()
	go func() { io.Copy(tunnel, local); done <- struct{}{} }()
	<-done
	tunnel.Close()
	local.Close()
	<-done

	s.mu.Lock()
	for i, x := range s.attached {
		if x == a {
			s.attached = append(s.attached[:i], s.attached[i+1:]...)
			break
		}
	}
	s.mu.Unlock()
	if err := s.VHCI.Detach(port); err != nil {
		// Usually already gone: the kernel detaches by itself when its socket closes.
		s.logf("detach port %d: %v", port, err)
	}
	s.logf("detached %s for client %s", busid, short(client))
	return nil
}

// DetachAll closes every tunnel (used on shutdown); Attach then detaches the ports.
func (s *Server) DetachAll() {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, a := range s.attached {
		a.conn.Close()
	}
}

func short(fp string) string {
	if len(fp) > 12 {
		return fp[:12]
	}
	return fp
}
