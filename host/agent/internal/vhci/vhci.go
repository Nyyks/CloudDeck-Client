// Package vhci drives the kernel's virtual USB host controller (vhci_hcd) through
// sysfs, the same interface `usbip attach` uses (drivers/usb/usbip/vhci_sysfs.c).
package vhci

import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
)

// DefaultRoot is the sysfs directory of the first vhci_hcd controller.
const DefaultRoot = "/sys/devices/platform/vhci_hcd.0"

// Port states (enum usbip_device_status).
const (
	StatusNull = 4 // VDEV_ST_NULL: port free
	StatusUsed = 6 // VDEV_ST_USED
)

// Port is one row of the status file.
type Port struct {
	Hub    string // "hs" (USB 2) or "ss" (USB 3)
	Port   int
	Status int
	Speed  int
	DevID  uint32
	BusID  string
}

// Free reports whether the port can take a new device.
func (p Port) Free() bool { return p.Status == StatusNull }

// ParseStatus parses the vhci status file:
//
//	hub port sta spd dev      sockfd local_busid
//	hs  0000 004 000 00000000 000000 0-0
func ParseStatus(text string) ([]Port, error) {
	var ports []Port
	sc := bufio.NewScanner(strings.NewReader(text))
	for sc.Scan() {
		f := strings.Fields(sc.Text())
		if len(f) == 0 || f[0] == "hub" {
			continue
		}
		if len(f) < 7 || (f[0] != "hs" && f[0] != "ss") {
			return nil, fmt.Errorf("vhci: unexpected status line %q", sc.Text())
		}
		port, err1 := strconv.Atoi(f[1])
		sta, err2 := strconv.Atoi(f[2])
		spd, err3 := strconv.Atoi(f[3])
		dev, err4 := strconv.ParseUint(f[4], 16, 32)
		if err := errors.Join(err1, err2, err3, err4); err != nil {
			return nil, fmt.Errorf("vhci: bad status line %q: %w", sc.Text(), err)
		}
		ports = append(ports, Port{Hub: f[0], Port: port, Status: sta, Speed: spd, DevID: uint32(dev), BusID: f[6]})
	}
	return ports, sc.Err()
}

// FreePort returns a free port on the hub matching the device speed class.
func FreePort(ports []Port, superSpeed bool) (int, error) {
	want := "hs"
	if superSpeed {
		want = "ss"
	}
	for _, p := range ports {
		if p.Hub == want && p.Free() {
			return p.Port, nil
		}
	}
	return -1, fmt.Errorf("vhci: no free %s port", want)
}

// Controller is a vhci_hcd instance in sysfs.
type Controller struct{ Root string }

// EnsureLoaded loads vhci-hcd if its sysfs directory is missing.
func (c Controller) EnsureLoaded() error {
	if _, err := os.Stat(c.Root); err == nil {
		return nil
	}
	if out, err := exec.Command("modprobe", "vhci-hcd").CombinedOutput(); err != nil {
		return fmt.Errorf("vhci: modprobe vhci-hcd: %v: %s", err, strings.TrimSpace(string(out)))
	}
	if _, err := os.Stat(c.Root); err != nil {
		return fmt.Errorf("vhci: %s missing after modprobe", c.Root)
	}
	return nil
}

// Ports reads the current port table.
func (c Controller) Ports() ([]Port, error) {
	b, err := os.ReadFile(filepath.Join(c.Root, "status"))
	if err != nil {
		return nil, err
	}
	return ParseStatus(string(b))
}

// Attach hands an already-imported USB/IP connection (sockfd, owned by this
// process) to the kernel on the given port.
func (c Controller) Attach(port int, sockfd uintptr, devid uint32, speed uint32) error {
	return writeSysfs(filepath.Join(c.Root, "attach"), fmt.Sprintf("%d %d %d %d", port, sockfd, devid, speed))
}

// Detach removes the device on port.
func (c Controller) Detach(port int) error {
	return writeSysfs(filepath.Join(c.Root, "detach"), strconv.Itoa(port))
}

// writeSysfs writes to an existing sysfs attribute; it never creates files.
func writeSysfs(path, value string) error {
	f, err := os.OpenFile(path, os.O_WRONLY, 0)
	if err != nil {
		return err
	}
	_, werr := f.WriteString(value)
	cerr := f.Close()
	return errors.Join(werr, cerr)
}
