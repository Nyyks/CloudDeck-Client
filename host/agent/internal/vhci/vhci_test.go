package vhci

import (
	"os"
	"path/filepath"
	"testing"
)

// Real status file layout from a kernel with 8 ports per hub (CONFIG_USBIP_VHCI_HC_PORTS=8).
const sampleStatus = `hub port sta spd dev      sockfd local_busid
hs  0000 006 002 00010005 000003 1-2
hs  0001 004 000 00000000 000000 0-0
hs  0002 004 000 00000000 000000 0-0
hs  0003 004 000 00000000 000000 0-0
hs  0004 004 000 00000000 000000 0-0
hs  0005 004 000 00000000 000000 0-0
hs  0006 004 000 00000000 000000 0-0
hs  0007 004 000 00000000 000000 0-0
ss  0008 004 000 00000000 000000 0-0
ss  0009 004 000 00000000 000000 0-0
ss  0010 004 000 00000000 000000 0-0
ss  0011 004 000 00000000 000000 0-0
ss  0012 004 000 00000000 000000 0-0
ss  0013 004 000 00000000 000000 0-0
ss  0014 004 000 00000000 000000 0-0
ss  0015 004 000 00000000 000000 0-0
`

func TestParseStatus(t *testing.T) {
	ports, err := ParseStatus(sampleStatus)
	if err != nil {
		t.Fatal(err)
	}
	if len(ports) != 16 {
		t.Fatalf("ports = %d", len(ports))
	}
	p := ports[0]
	if p.Hub != "hs" || p.Port != 0 || p.Status != StatusUsed || p.Speed != 2 || p.DevID != 0x10005 || p.BusID != "1-2" || p.Free() {
		t.Fatalf("port 0 = %+v", p)
	}
	if !ports[1].Free() || ports[8].Hub != "ss" || ports[8].Port != 8 {
		t.Fatalf("ports 1/8 = %+v %+v", ports[1], ports[8])
	}
}

func TestParseStatusRejectsGarbage(t *testing.T) {
	if _, err := ParseStatus("hub port\nxx 1 2 3 4 5 6\n"); err == nil {
		t.Fatal("garbage accepted")
	}
	if _, err := ParseStatus("hs 0000 zzz 000 00000000 000000 0-0\n"); err == nil {
		t.Fatal("bad number accepted")
	}
}

func TestFreePort(t *testing.T) {
	ports, _ := ParseStatus(sampleStatus)
	if p, err := FreePort(ports, false); err != nil || p != 1 {
		t.Fatalf("hs free = %d %v (port 0 is used)", p, err)
	}
	if p, err := FreePort(ports, true); err != nil || p != 8 {
		t.Fatalf("ss free = %d %v", p, err)
	}
	for i := range ports {
		if ports[i].Hub == "hs" {
			ports[i].Status = StatusUsed
		}
	}
	if _, err := FreePort(ports, false); err == nil {
		t.Fatal("no free hs port should error")
	}
}

func TestAttachDetachWriteSysfsFormat(t *testing.T) {
	root := t.TempDir()
	c := Controller{Root: root}
	for name, content := range map[string]string{"status": sampleStatus, "attach": "", "detach": ""} {
		if err := os.WriteFile(filepath.Join(root, name), []byte(content), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	if err := c.Attach(3, 7, 0x10005, 3); err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(filepath.Join(root, "attach")); string(b) != "3 7 65541 3" {
		t.Fatalf("attach wrote %q", b)
	}
	if err := c.Detach(3); err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(filepath.Join(root, "detach")); string(b) != "3" {
		t.Fatalf("detach wrote %q", b)
	}
	if ports, err := c.Ports(); err != nil || len(ports) != 16 {
		t.Fatalf("ports: %v %v", len(ports), err)
	}
	if err := (Controller{Root: root}).EnsureLoaded(); err != nil {
		t.Fatalf("EnsureLoaded on existing dir: %v", err)
	}
	// A missing sysfs attribute is an error, never a newly created file.
	if err := (Controller{Root: t.TempDir()}).Detach(1); err == nil {
		t.Fatal("detach on missing sysfs file should fail")
	}
}
