// clouddeck-usb-helper: Linux *client-side* helper for USB forwarding
// (docs/adr/0006-usb-passthrough.md). The desktop app starts it with pkexec once
// per stream; it runs as root only to drive the kernel's usbip-host driver.
//
// Protocol on stdin/stdout (one line each):
//
//	app -> helper:  export <busid>   |  release <busid>
//	helper -> app:  ready <busid> <unix socket path>  |  error <busid> <message>  |  released <busid>
//
// Each exported device gets a Unix socket owned by the invoking user (0600). The
// app connects it to the host agent's tunnel; the helper answers the USB/IP import
// and hands the device to the kernel. Nothing listens on the network. When stdin
// closes (app exit or crash), every device is given back to its normal driver.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/stub"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/usbip"
)

var version = "dev"

type exported struct {
	listener net.Listener
	path     string
}

func main() {
	uidFlag := flag.Int("uid", -1, "user that may connect (default: $PKEXEC_UID)")
	sysRoot := flag.String("sysfs", "/sys", "sysfs root (tests)")
	flag.Parse()
	logger := log.New(os.Stderr, "clouddeck-usb-helper: ", 0)

	uid := *uidFlag
	if v := os.Getenv("PKEXEC_UID"); v != "" {
		uid, _ = strconv.Atoi(v) // pkexec sets this; it can't be spoofed by the caller
	}
	if uid < 0 {
		logger.Fatal("unknown invoking user: run through pkexec or pass -uid")
	}
	if os.Geteuid() != 0 {
		logger.Fatal("must run as root (pkexec)")
	}
	dir := fmt.Sprintf("/run/user/%d/clouddeck-usb", uid)
	if err := os.MkdirAll(dir, 0o700); err != nil {
		logger.Fatal(err)
	}
	_ = os.Chown(dir, uid, -1)
	_ = os.Chmod(dir, 0o700)

	sys := stub.Sysfs{Root: *sysRoot}
	if err := sys.EnsureLoaded(); err != nil {
		fmt.Printf("error * %v\n", err)
	}

	var mu sync.Mutex
	devices := map[string]*exported{}
	out := bufio.NewWriter(os.Stdout)
	say := func(format string, a ...any) {
		mu.Lock()
		defer mu.Unlock()
		fmt.Fprintf(out, format+"\n", a...)
		out.Flush()
	}

	release := func(busid string) {
		mu.Lock()
		e := devices[busid]
		delete(devices, busid)
		mu.Unlock()
		if e == nil {
			return
		}
		e.listener.Close()
		_ = os.Remove(e.path)
		if err := sys.Release(busid); err != nil {
			logger.Printf("release %s: %v", busid, err)
		}
	}

	export := func(busid string) {
		if !usbip.ValidBusID(busid) {
			say("error %s invalid busid", busid)
			return
		}
		mu.Lock()
		_, dup := devices[busid]
		mu.Unlock()
		if dup {
			say("error %s already exported", busid)
			return
		}
		if err := sys.Bind(busid); err != nil {
			say("error %s %v", busid, err)
			return
		}
		path := filepath.Join(dir, busid+".sock")
		_ = os.Remove(path)
		l, err := net.Listen("unix", path)
		if err == nil {
			err = os.Chown(path, uid, -1)
		}
		if err == nil {
			err = os.Chmod(path, 0o600)
		}
		if err != nil {
			if l != nil {
				l.Close()
			}
			_ = sys.Release(busid)
			say("error %s %v", busid, err)
			return
		}
		mu.Lock()
		devices[busid] = &exported{listener: l, path: path}
		mu.Unlock()
		go func() {
			for {
				c, err := l.Accept()
				if err != nil {
					return
				}
				if err := stub.ServeImport(sys, busid, c); err != nil {
					logger.Printf("%s: %v", busid, err)
				}
			}
		}()
		say("ready %s %s", busid, path)
	}

	logger.Printf("%s started for uid %d", version, uid)
	sc := bufio.NewScanner(os.Stdin)
	for sc.Scan() {
		f := strings.Fields(sc.Text())
		if len(f) != 2 {
			continue
		}
		switch f[0] {
		case "export":
			export(f[1])
		case "release":
			release(f[1])
			say("released %s", f[1])
		}
	}
	// stdin closed: the app is done (or gone). Give everything back.
	mu.Lock()
	ids := make([]string, 0, len(devices))
	for id := range devices {
		ids = append(ids, id)
	}
	mu.Unlock()
	for _, id := range ids {
		release(id)
	}
	logger.Printf("all devices released")
}
