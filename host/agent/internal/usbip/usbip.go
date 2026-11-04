// Package usbip implements the parts of the USB/IP wire protocol the host agent
// speaks: OP_REQ_IMPORT / OP_REP_IMPORT (Linux Documentation/usb/usbip_protocol.rst).
// After a successful import the stream carries USBIP_CMD/RET_* messages, which the
// kernel's vhci_hcd handles directly.
package usbip

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
)

const (
	Version     = 0x0111
	OpReqImport = 0x8003
	OpRepImport = 0x0003

	busIDSize  = 32
	pathSize   = 256
	headerSize = 8   // version u16, command u16, status u32
	deviceSize = 312 // struct usbip_usb_device
	StatusOK   = 0
)

// USB speeds (enum usb_device_speed).
const (
	SpeedUnknown   = 0
	SpeedLow       = 1
	SpeedFull      = 2
	SpeedHigh      = 3
	SpeedWireless  = 4
	SpeedSuper     = 5
	SpeedSuperPlus = 6
)

// Device is struct usbip_usb_device.
type Device struct {
	Path               string
	BusID              string
	BusNum             uint32
	DevNum             uint32
	Speed              uint32
	IDVendor           uint16
	IDProduct          uint16
	BcdDevice          uint16
	DeviceClass        uint8
	DeviceSubClass     uint8
	DeviceProtocol     uint8
	ConfigurationValue uint8
	NumConfigurations  uint8
	NumInterfaces      uint8
}

// DevID is the value vhci_hcd expects in its attach file: busnum << 16 | devnum.
func (d Device) DevID() uint32 { return d.BusNum<<16 | d.DevNum }

// IsSuperSpeed reports whether the device needs a USB 3 ("ss") vhci port.
func (d Device) IsSuperSpeed() bool { return d.Speed >= SpeedSuper }

var (
	ErrBadVersion = errors.New("usbip: unsupported protocol version")
	ErrBadCommand = errors.New("usbip: unexpected command")
)

// ImportError is returned when the exporting side rejects the import.
type ImportError struct{ Status uint32 }

func (e ImportError) Error() string {
	return fmt.Sprintf("usbip: import refused by exporter (status %d)", e.Status)
}

// ValidBusID reports whether busid looks like a USB/IP bus id ("1-2", "1-2.3", "3-1.4.2").
// It must fit the 32-byte field with a terminating NUL and contain only safe characters.
func ValidBusID(busid string) bool {
	if len(busid) == 0 || len(busid) >= busIDSize {
		return false
	}
	for _, c := range busid {
		if !(c >= '0' && c <= '9') && c != '-' && c != '.' {
			return false
		}
	}
	return true
}

// WriteImportRequest sends OP_REQ_IMPORT for busid.
func WriteImportRequest(w io.Writer, busid string) error {
	if !ValidBusID(busid) {
		return fmt.Errorf("usbip: invalid busid %q", busid)
	}
	buf := make([]byte, headerSize+busIDSize)
	binary.BigEndian.PutUint16(buf[0:], Version)
	binary.BigEndian.PutUint16(buf[2:], OpReqImport)
	binary.BigEndian.PutUint32(buf[4:], StatusOK)
	copy(buf[headerSize:], busid)
	_, err := w.Write(buf)
	return err
}

// ReadImportRequest parses OP_REQ_IMPORT (used by tests and fake exporters).
func ReadImportRequest(r io.Reader) (string, error) {
	buf := make([]byte, headerSize+busIDSize)
	if _, err := io.ReadFull(r, buf); err != nil {
		return "", err
	}
	if binary.BigEndian.Uint16(buf[0:]) != Version {
		return "", ErrBadVersion
	}
	if binary.BigEndian.Uint16(buf[2:]) != OpReqImport {
		return "", ErrBadCommand
	}
	return cstring(buf[headerSize:]), nil
}

// ReadImportReply parses OP_REP_IMPORT and returns the imported device.
func ReadImportReply(r io.Reader) (Device, error) {
	hdr := make([]byte, headerSize)
	if _, err := io.ReadFull(r, hdr); err != nil {
		return Device{}, err
	}
	if binary.BigEndian.Uint16(hdr[0:]) != Version {
		return Device{}, ErrBadVersion
	}
	if binary.BigEndian.Uint16(hdr[2:]) != OpRepImport {
		return Device{}, ErrBadCommand
	}
	if st := binary.BigEndian.Uint32(hdr[4:]); st != StatusOK {
		return Device{}, ImportError{Status: st}
	}
	body := make([]byte, deviceSize)
	if _, err := io.ReadFull(r, body); err != nil {
		return Device{}, err
	}
	return DecodeDevice(body)
}

// WriteImportReply sends OP_REP_IMPORT (used by tests and fake exporters).
func WriteImportReply(w io.Writer, status uint32, d Device) error {
	hdr := make([]byte, headerSize)
	binary.BigEndian.PutUint16(hdr[0:], Version)
	binary.BigEndian.PutUint16(hdr[2:], OpRepImport)
	binary.BigEndian.PutUint32(hdr[4:], status)
	if _, err := w.Write(hdr); err != nil || status != StatusOK {
		return err
	}
	_, err := w.Write(EncodeDevice(d))
	return err
}

// EncodeDevice serializes struct usbip_usb_device (312 bytes, big-endian).
func EncodeDevice(d Device) []byte {
	b := make([]byte, deviceSize)
	copy(b[0:pathSize-1], d.Path)
	copy(b[pathSize:pathSize+busIDSize-1], d.BusID)
	o := pathSize + busIDSize
	binary.BigEndian.PutUint32(b[o:], d.BusNum)
	binary.BigEndian.PutUint32(b[o+4:], d.DevNum)
	binary.BigEndian.PutUint32(b[o+8:], d.Speed)
	binary.BigEndian.PutUint16(b[o+12:], d.IDVendor)
	binary.BigEndian.PutUint16(b[o+14:], d.IDProduct)
	binary.BigEndian.PutUint16(b[o+16:], d.BcdDevice)
	b[o+18] = d.DeviceClass
	b[o+19] = d.DeviceSubClass
	b[o+20] = d.DeviceProtocol
	b[o+21] = d.ConfigurationValue
	b[o+22] = d.NumConfigurations
	b[o+23] = d.NumInterfaces
	return b
}

// DecodeDevice parses struct usbip_usb_device.
func DecodeDevice(b []byte) (Device, error) {
	if len(b) != deviceSize {
		return Device{}, fmt.Errorf("usbip: device record is %d bytes, want %d", len(b), deviceSize)
	}
	o := pathSize + busIDSize
	return Device{
		Path:               cstring(b[0:pathSize]),
		BusID:              cstring(b[pathSize : pathSize+busIDSize]),
		BusNum:             binary.BigEndian.Uint32(b[o:]),
		DevNum:             binary.BigEndian.Uint32(b[o+4:]),
		Speed:              binary.BigEndian.Uint32(b[o+8:]),
		IDVendor:           binary.BigEndian.Uint16(b[o+12:]),
		IDProduct:          binary.BigEndian.Uint16(b[o+14:]),
		BcdDevice:          binary.BigEndian.Uint16(b[o+16:]),
		DeviceClass:        b[o+18],
		DeviceSubClass:     b[o+19],
		DeviceProtocol:     b[o+20],
		ConfigurationValue: b[o+21],
		NumConfigurations:  b[o+22],
		NumInterfaces:      b[o+23],
	}, nil
}

func cstring(b []byte) string {
	if i := bytes.IndexByte(b, 0); i >= 0 {
		b = b[:i]
	}
	return string(b)
}
