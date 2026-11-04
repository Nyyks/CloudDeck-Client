// Package stub exports local USB devices through the kernel's usbip-host driver
// (drivers/usb/usbip/stub_*.c), the same sysfs interface `usbip bind` and usbipd
// use. Used by the Linux client's clouddeck-usb-helper (docs/adr/0006).
package stub

import (
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/usbip"
)

// Sysfs is a sysfs tree ("/sys" in production, a temp dir in tests).
type Sysfs struct{ Root string }

func (s Sysfs) dev(busid string, attr string) string {
	return filepath.Join(s.Root, "bus/usb/devices", busid, attr)
}

func (s Sysfs) drv(attr string) string {
	return filepath.Join(s.Root, "bus/usb/drivers/usbip-host", attr)
}

func (s Sysfs) readAttr(busid, attr string) (string, error) {
	b, err := os.ReadFile(s.dev(busid, attr))
	return strings.TrimSpace(string(b)), err
}

func write(path, value string) error {
	f, err := os.OpenFile(path, os.O_WRONLY, 0) // sysfs attribute: never create
	if err != nil {
		return err
	}
	_, werr := f.WriteString(value)
	return errors.Join(werr, f.Close())
}

// speedFromSysfs maps the sysfs "speed" attribute (Mbit/s) to enum usb_device_speed.
func speedFromSysfs(s string) uint32 {
	switch s {
	case "1.5":
		return usbip.SpeedLow
	case "12":
		return usbip.SpeedFull
	case "480":
		return usbip.SpeedHigh
	case "53.3-480":
		return usbip.SpeedWireless
	case "5000":
		return usbip.SpeedSuper
	case "10000", "20000":
		return usbip.SpeedSuperPlus
	}
	return usbip.SpeedUnknown
}

// DeviceInfo builds the OP_REP_IMPORT device record from sysfs.
func (s Sysfs) DeviceInfo(busid string) (usbip.Device, error) {
	if !usbip.ValidBusID(busid) {
		return usbip.Device{}, fmt.Errorf("invalid busid %q", busid)
	}
	dir := filepath.Join(s.Root, "bus/usb/devices", busid)
	path, err := filepath.EvalSymlinks(dir)
	if err != nil {
		return usbip.Device{}, err
	}
	var errs []error
	dec := func(attr string) uint32 {
		v, err := s.readAttr(busid, attr)
		n, perr := strconv.ParseUint(v, 10, 32)
		errs = append(errs, err, perr)
		return uint32(n)
	}
	hex := func(attr string) uint32 {
		v, err := s.readAttr(busid, attr)
		n, perr := strconv.ParseUint(v, 16, 32)
		errs = append(errs, err, perr)
		return uint32(n)
	}
	speed, err := s.readAttr(busid, "speed")
	errs = append(errs, err)
	// Production sysfs lives under /sys; report that path to the importer.
	reportedPath := path
	if s.Root != "/sys" {
		reportedPath = "/sys" + strings.TrimPrefix(path, s.Root)
	}
	d := usbip.Device{
		Path:               reportedPath,
		BusID:              busid,
		BusNum:             dec("busnum"),
		DevNum:             dec("devnum"),
		Speed:              speedFromSysfs(speed),
		IDVendor:           uint16(hex("idVendor")),
		IDProduct:          uint16(hex("idProduct")),
		BcdDevice:          uint16(hex("bcdDevice")),
		DeviceClass:        uint8(hex("bDeviceClass")),
		DeviceSubClass:     uint8(hex("bDeviceSubClass")),
		DeviceProtocol:     uint8(hex("bDeviceProtocol")),
		ConfigurationValue: uint8(dec("bConfigurationValue")),
		NumConfigurations:  uint8(dec("bNumConfigurations")),
		NumInterfaces:      uint8(dec("bNumInterfaces")),
	}
	if err := errors.Join(errs...); err != nil {
		return usbip.Device{}, fmt.Errorf("read %s from sysfs: %w", busid, err)
	}
	return d, nil
}

// EnsureLoaded loads usbip-host if needed.
func (s Sysfs) EnsureLoaded() error {
	if _, err := os.Stat(s.drv("")); err == nil {
		return nil
	}
	if out, err := exec.Command("modprobe", "usbip-host").CombinedOutput(); err != nil {
		return fmt.Errorf("modprobe usbip-host: %v: %s", err, strings.TrimSpace(string(out)))
	}
	return nil
}

// Bind detaches busid from its current driver and binds it to usbip-host
// (what `usbip bind -b busid` does). The device disappears locally.
func (s Sysfs) Bind(busid string) error {
	if !usbip.ValidBusID(busid) {
		return fmt.Errorf("invalid busid %q", busid)
	}
	if err := write(s.drv("match_busid"), "add "+busid); err != nil {
		return fmt.Errorf("match_busid add: %w", err)
	}
	if target, err := os.Readlink(s.dev(busid, "driver")); err == nil && filepath.Base(target) != "usbip-host" {
		if err := write(s.dev(busid, "driver/unbind"), busid); err != nil {
			return fmt.Errorf("unbind from %s: %w", filepath.Base(target), err)
		}
	}
	if err := write(s.drv("bind"), busid); err != nil {
		return fmt.Errorf("bind to usbip-host: %w", err)
	}
	return nil
}

// Export hands an open socket to the stub driver for busid.
func (s Sysfs) Export(busid string, sockfd uintptr) error {
	return write(s.dev(busid, "usbip_sockfd"), strconv.FormatUint(uint64(sockfd), 10))
}

// Release gives busid back to its normal driver (what `usbip unbind` does).
func (s Sysfs) Release(busid string) error {
	err1 := write(s.drv("unbind"), busid)
	err2 := write(s.drv("match_busid"), "del "+busid)
	err3 := write(s.drv("rebind"), busid)
	if err3 == nil {
		return nil // rebind is what matters: the device is back with its driver
	}
	return errors.Join(err1, err2, err3)
}

// Exporter is the subset of Sysfs ServeImport needs (faked in tests).
type Exporter interface {
	DeviceInfo(busid string) (usbip.Device, error)
	Export(busid string, sockfd uintptr) error
}

// ServeImport answers one OP_REQ_IMPORT for busid on conn, hands the device to the
// kernel, and relays between conn and the kernel's socket until either side closes.
func ServeImport(ex Exporter, busid string, conn net.Conn) error {
	defer conn.Close()
	req, err := usbip.ReadImportRequest(conn)
	if err != nil {
		return err
	}
	if req != busid {
		_ = usbip.WriteImportReply(conn, 1, usbip.Device{})
		return fmt.Errorf("import of %q requested on the socket for %q", req, busid)
	}
	dev, err := ex.DeviceInfo(busid)
	if err != nil {
		_ = usbip.WriteImportReply(conn, 1, usbip.Device{})
		return err
	}
	fds, err := syscall.Socketpair(syscall.AF_UNIX, syscall.SOCK_STREAM|syscall.SOCK_CLOEXEC, 0)
	if err != nil {
		return err
	}
	kernelEnd := os.NewFile(uintptr(fds[0]), "stub-kernel")
	ourEnd := os.NewFile(uintptr(fds[1]), "stub-helper")
	local, err := net.FileConn(ourEnd)
	ourEnd.Close()
	if err != nil {
		kernelEnd.Close()
		return err
	}
	defer local.Close()
	err = ex.Export(busid, kernelEnd.Fd())
	kernelEnd.Close()
	if err != nil {
		_ = usbip.WriteImportReply(conn, 1, usbip.Device{})
		return fmt.Errorf("export %s: %w", busid, err)
	}
	// Like usbipd: export first, then reply. The kernel only sends after commands arrive.
	if err := usbip.WriteImportReply(conn, usbip.StatusOK, dev); err != nil {
		return err
	}
	done := make(chan struct{}, 2)
	go func() { io.Copy(local, conn); done <- struct{}{} }()
	go func() { io.Copy(conn, local); done <- struct{}{} }()
	<-done
	conn.Close()
	local.Close()
	<-done
	return nil
}
