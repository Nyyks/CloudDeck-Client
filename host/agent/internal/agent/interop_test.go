package agent

import (
	"io"
	"net"
	"os"
	"syscall"
	"testing"
	"time"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/stub"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/usbip"
)

// clientKernel fakes the Linux client's usbip-host stub (stub.Exporter).
type clientKernel struct {
	dev  usbip.Device
	sock chan net.Conn
}

func (k *clientKernel) DeviceInfo(string) (usbip.Device, error) { return k.dev, nil }
func (k *clientKernel) Export(_ string, fd uintptr) error {
	dup, err := syscall.Dup(int(fd))
	if err != nil {
		return err
	}
	c, err := net.FileConn(os.NewFile(uintptr(dup), "client-kernel"))
	if err != nil {
		return err
	}
	k.sock <- c
	return nil
}

// The host agent (importer) talks directly to the Linux helper's exporter, as
// they do through the tunnel. Each side's "kernel" is faked; URB bytes must flow.
func TestAgentImportsFromLinuxHelper(t *testing.T) {
	hostVHCI := newFake()
	agentSrv := &Server{VHCI: hostVHCI}
	ck := &clientKernel{
		dev:  usbip.Device{BusID: "3-1.2", BusNum: 3, DevNum: 7, Speed: usbip.SpeedSuper, IDVendor: 0x0781, IDProduct: 0x5583},
		sock: make(chan net.Conn, 1),
	}
	agentSide, helperSide := net.Pipe()
	go agentSrv.Attach(fp, "3-1.2", agentSide, agentSide)
	go stub.ServeImport(ck, "3-1.2", helperSide)

	var hostKernel, devKernel net.Conn
	for hostKernel == nil || devKernel == nil {
		select {
		case hostKernel = <-hostVHCI.attached:
		case devKernel = <-ck.sock:
		case <-time.After(3 * time.Second):
			t.Fatal("import handshake did not complete")
		}
	}
	if got := hostVHCI.lastArgs; got[0] != 8 || got[1] != 3<<16|7 || got[2] != usbip.SpeedSuper {
		t.Fatalf("vhci attach port/devid/speed = %v (USB 3 device must take an ss port)", got)
	}

	exchange := func(from, to net.Conn, msg string) {
		go from.Write([]byte(msg))
		buf := make([]byte, len(msg))
		to.SetReadDeadline(time.Now().Add(3 * time.Second))
		if _, err := io.ReadFull(to, buf); err != nil || string(buf) != msg {
			t.Fatalf("%s: got %q %v", msg, buf, err)
		}
	}
	exchange(hostKernel, devKernel, "USBIP_CMD_SUBMIT")
	exchange(devKernel, hostKernel, "USBIP_RET_SUBMIT")

	// Client side goes away -> host detaches.
	devKernel.Close()
	helperSide.Close()
	select {
	case <-hostVHCI.detached:
	case <-time.After(3 * time.Second):
		t.Fatal("host did not detach when the client went away")
	}
}
