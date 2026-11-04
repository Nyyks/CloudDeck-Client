package usbip

import (
	"bytes"
	"encoding/hex"
	"errors"
	"testing"
)

func TestImportRequestLayout(t *testing.T) {
	var b bytes.Buffer
	if err := WriteImportRequest(&b, "1-2.3"); err != nil {
		t.Fatal(err)
	}
	got := b.Bytes()
	if len(got) != 40 {
		t.Fatalf("len = %d, want 40", len(got))
	}
	// version 0x0111, command 0x8003, status 0, then "1-2.3" NUL-padded to 32 bytes
	if hex.EncodeToString(got[:8]) != "0111800300000000" {
		t.Fatalf("header = %x", got[:8])
	}
	if string(got[8:13]) != "1-2.3" || got[13] != 0 || got[39] != 0 {
		t.Fatalf("busid field = %q", got[8:])
	}
	busid, err := ReadImportRequest(bytes.NewReader(got))
	if err != nil || busid != "1-2.3" {
		t.Fatalf("round trip: %q %v", busid, err)
	}
}

func TestBusIDValidation(t *testing.T) {
	for _, ok := range []string{"1-1", "1-2.3", "3-1.4.2", "12-10"} {
		if !ValidBusID(ok) {
			t.Errorf("%q should be valid", ok)
		}
	}
	for _, bad := range []string{"", "1-2;rm", "../1", "1 2", "a-b", string(make([]byte, 32))} {
		if ValidBusID(bad) {
			t.Errorf("%q should be invalid", bad)
		}
	}
	if err := WriteImportRequest(&bytes.Buffer{}, "1-2\n"); err == nil {
		t.Error("newline busid accepted")
	}
}

func sampleDevice() Device {
	return Device{
		Path: "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-2", BusID: "1-2",
		BusNum: 1, DevNum: 5, Speed: SpeedHigh,
		IDVendor: 0x045e, IDProduct: 0x028e, BcdDevice: 0x0114,
		DeviceClass: 0xff, DeviceSubClass: 0xff, DeviceProtocol: 0xff,
		ConfigurationValue: 1, NumConfigurations: 1, NumInterfaces: 4,
	}
}

func TestDeviceRoundTripAndLayout(t *testing.T) {
	d := sampleDevice()
	b := EncodeDevice(d)
	if len(b) != 312 {
		t.Fatalf("len = %d", len(b))
	}
	// busnum at 288, devnum at 292, speed at 296, idVendor at 300 (big-endian)
	if hex.EncodeToString(b[288:302]) != "0000000100000005"+"00000003"+"045e" {
		t.Fatalf("fields = %x", b[288:302])
	}
	got, err := DecodeDevice(b)
	if err != nil || got != d {
		t.Fatalf("round trip: %+v %v", got, err)
	}
	if d.DevID() != 1<<16|5 {
		t.Fatalf("devid = %x", d.DevID())
	}
	if d.IsSuperSpeed() || !(Device{Speed: SpeedSuper}).IsSuperSpeed() {
		t.Fatal("IsSuperSpeed")
	}
}

func TestImportReplyOK(t *testing.T) {
	var b bytes.Buffer
	if err := WriteImportReply(&b, StatusOK, sampleDevice()); err != nil {
		t.Fatal(err)
	}
	if b.Len() != 8+312 {
		t.Fatalf("len = %d", b.Len())
	}
	d, err := ReadImportReply(&b)
	if err != nil || d != sampleDevice() {
		t.Fatalf("%+v %v", d, err)
	}
}

func TestImportReplyRefusedAndMalformed(t *testing.T) {
	var b bytes.Buffer
	_ = WriteImportReply(&b, 1, Device{})
	var ie ImportError
	if _, err := ReadImportReply(&b); !errors.As(err, &ie) || ie.Status != 1 {
		t.Fatalf("want ImportError, got %v", err)
	}
	if _, err := ReadImportReply(bytes.NewReader([]byte{0x01, 0x06, 0, 3, 0, 0, 0, 0})); err != ErrBadVersion {
		t.Fatalf("bad version: %v", err)
	}
	if _, err := ReadImportReply(bytes.NewReader([]byte{0x01, 0x11, 0x80, 3, 0, 0, 0, 0})); err != ErrBadCommand {
		t.Fatalf("bad command: %v", err)
	}
	short := append([]byte{0x01, 0x11, 0, 3, 0, 0, 0, 0}, make([]byte, 100)...)
	if _, err := ReadImportReply(bytes.NewReader(short)); err == nil {
		t.Fatal("truncated device accepted")
	}
}
