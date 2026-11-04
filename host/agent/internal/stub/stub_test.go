package stub

import (
	"io"
	"net"
	"os"
	"path/filepath"
	"syscall"
	"testing"
	"time"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/usbip"
)

// fakeSysfs builds a sysfs tree with one device "1-2" bound to driver "usb".
func fakeSysfs(t *testing.T) Sysfs {
	root := t.TempDir()
	real := filepath.Join(root, "devices/pci0000:00/0000:00:14.0/usb1/1-2")
	attrs := map[string]string{
		"busnum": "1", "devnum": "5", "speed": "480",
		"idVendor": "045e", "idProduct": "028e", "bcdDevice": "0114",
		"bDeviceClass": "ff", "bDeviceSubClass": "ff", "bDeviceProtocol": "ff",
		"bConfigurationValue": "1", "bNumConfigurations": "1", "bNumInterfaces": " 4",
		"usbip_sockfd": "",
	}
	must := func(err error) {
		t.Helper()
		if err != nil {
			t.Fatal(err)
		}
	}
	must(os.MkdirAll(real, 0o755))
	for k, v := range attrs {
		must(os.WriteFile(filepath.Join(real, k), []byte(v+"\n"), 0o644))
	}
	usbDrv := filepath.Join(root, "bus/usb/drivers/usb")
	stubDrv := filepath.Join(root, "bus/usb/drivers/usbip-host")
	must(os.MkdirAll(usbDrv, 0o755))
	must(os.MkdirAll(stubDrv, 0o755))
	for _, f := range []string{"unbind", "bind"} {
		must(os.WriteFile(filepath.Join(usbDrv, f), nil, 0o644))
	}
	for _, f := range []string{"match_busid", "bind", "unbind", "rebind"} {
		must(os.WriteFile(filepath.Join(stubDrv, f), nil, 0o644))
	}
	must(os.Symlink(usbDrv, filepath.Join(real, "driver")))
	must(os.MkdirAll(filepath.Join(root, "bus/usb/devices"), 0o755))
	must(os.Symlink(real, filepath.Join(root, "bus/usb/devices/1-2")))
	return Sysfs{Root: root}
}

func read(t *testing.T, p string) string {
	b, err := os.ReadFile(p)
	if err != nil {
		t.Fatal(err)
	}
	return string(b)
}

func TestDeviceInfoFromSysfs(t *testing.T) {
	s := fakeSysfs(t)
	d, err := s.DeviceInfo("1-2")
	if err != nil {
		t.Fatal(err)
	}
	want := usbip.Device{
		Path: "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2", BusID: "1-2",
		BusNum: 1, DevNum: 5, Speed: usbip.SpeedHigh, IDVendor: 0x045e, IDProduct: 0x028e, BcdDevice: 0x0114,
		DeviceClass: 0xff, DeviceSubClass: 0xff, DeviceProtocol: 0xff,
		ConfigurationValue: 1, NumConfigurations: 1, NumInterfaces: 4,
	}
	if d != want {
		t.Fatalf("got  %+v\nwant %+v", d, want)
	}
	if _, err := s.DeviceInfo("9-9"); err == nil {
		t.Fatal("missing device accepted")
	}
	if _, err := s.DeviceInfo("../x"); err == nil {
		t.Fatal("bad busid accepted")
	}
}

func TestSpeedMapping(t *testing.T) {
	for in, want := range map[string]uint32{"1.5": 1, "12": 2, "480": 3, "5000": 5, "10000": 6, "20000": 6, "x": 0} {
		if got := speedFromSysfs(in); got != want {
			t.Errorf("%s -> %d, want %d", in, got, want)
		}
	}
}

func TestBindAndReleaseSequence(t *testing.T) {
	s := fakeSysfs(t)
	if err := s.Bind("1-2"); err != nil {
		t.Fatal(err)
	}
	if got := read(t, s.drv("match_busid")); got != "add 1-2" {
		t.Fatalf("match_busid = %q", got)
	}
	if got := read(t, filepath.Join(s.Root, "bus/usb/drivers/usb/unbind")); got != "1-2" {
		t.Fatalf("old driver unbind = %q", got)
	}
	if got := read(t, s.drv("bind")); got != "1-2" {
		t.Fatalf("usbip-host bind = %q", got)
	}
	if err := s.Release("1-2"); err != nil {
		t.Fatal(err)
	}
	if got := read(t, s.drv("rebind")); got != "1-2" {
		t.Fatalf("rebind = %q", got)
	}
	if got := read(t, s.drv("match_busid")); got != "del 1-2" {
		t.Fatalf("match_busid after release = %q", got)
	}
	if err := s.Bind("1-2;x"); err == nil {
		t.Fatal("bad busid accepted")
	}
}

type fakeExporter struct {
	dev    usbip.Device
	kernel chan net.Conn
}

func (f *fakeExporter) DeviceInfo(string) (usbip.Device, error) { return f.dev, nil }
func (f *fakeExporter) Export(_ string, fd uintptr) error {
	dup, err := syscall.Dup(int(fd))
	if err != nil {
		return err
	}
	c, err := net.FileConn(os.NewFile(uintptr(dup), "kernel"))
	if err != nil {
		return err
	}
	f.kernel <- c
	return nil
}

func TestServeImportExportsThenRelays(t *testing.T) {
	ex := &fakeExporter{dev: usbip.Device{BusID: "1-2", BusNum: 1, DevNum: 5, Speed: usbip.SpeedHigh}, kernel: make(chan net.Conn, 1)}
	importer, helper := net.Pipe()
	errc := make(chan error, 1)
	go func() { errc <- ServeImport(ex, "1-2", helper) }()

	if err := usbip.WriteImportRequest(importer, "1-2"); err != nil {
		t.Fatal(err)
	}
	dev, err := usbip.ReadImportReply(importer)
	if err != nil || dev.DevID() != 1<<16|5 {
		t.Fatalf("reply %+v %v", dev, err)
	}
	kernel := <-ex.kernel
	// importer -> kernel
	go importer.Write([]byte("CMD_SUBMIT"))
	buf := make([]byte, 10)
	kernel.SetReadDeadline(time.Now().Add(3 * time.Second))
	if _, err := io.ReadFull(kernel, buf); err != nil || string(buf) != "CMD_SUBMIT" {
		t.Fatalf("to kernel: %q %v", buf, err)
	}
	// kernel -> importer
	go kernel.Write([]byte("RET_SUBMIT"))
	importer.SetReadDeadline(time.Now().Add(3 * time.Second))
	if _, err := io.ReadFull(importer, buf); err != nil || string(buf) != "RET_SUBMIT" {
		t.Fatalf("to importer: %q %v", buf, err)
	}
	importer.Close()
	kernel.Close()
	select {
	case <-errc:
	case <-time.After(3 * time.Second):
		t.Fatal("ServeImport did not return after close")
	}
}

func TestServeImportRejectsOtherBusID(t *testing.T) {
	ex := &fakeExporter{dev: usbip.Device{BusID: "1-2"}, kernel: make(chan net.Conn, 1)}
	importer, helper := net.Pipe()
	go ServeImport(ex, "1-2", helper)
	usbip.WriteImportRequest(importer, "3-1")
	if _, err := usbip.ReadImportReply(importer); err == nil {
		t.Fatal("import of another device accepted")
	}
	select {
	case <-ex.kernel:
		t.Fatal("exported despite busid mismatch")
	default:
	}
}
