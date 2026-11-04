// clouddeck-agent: CloudDeck-Client host agent (runs as root, systemd system unit).
// Reached only through Sunshine's /clouddeck/v1/* tunnel (patch 0002) over a Unix
// socket. See docs/adr/0006-usb-passthrough.md.
package main

import (
	"flag"
	"log"
	"net"
	"os"
	"os/signal"
	"os/user"
	"path/filepath"
	"strconv"
	"syscall"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/agent"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/vhci"
)

var version = "dev" // set with -ldflags "-X main.version=..."

func main() {
	socketPath := flag.String("socket", "/run/clouddeck-client/agent.sock", "Unix socket Sunshine connects to")
	group := flag.String("group", "", "group allowed to connect (the desktop user's group, which Sunshine runs as)")
	vhciRoot := flag.String("vhci", vhci.DefaultRoot, "vhci_hcd sysfs directory")
	flag.Parse()

	logger := log.New(os.Stderr, "", 0) // journald adds timestamps
	logger.Printf("clouddeck-agent %s starting", version)

	ctl := vhci.Controller{Root: *vhciRoot}
	if err := ctl.EnsureLoaded(); err != nil {
		// Keep running: hello still works and the UI can show why USB is unavailable.
		logger.Printf("warning: %v", err)
	}

	if err := os.MkdirAll(filepath.Dir(*socketPath), 0o755); err != nil {
		logger.Fatal(err)
	}
	_ = os.Remove(*socketPath)
	l, err := net.Listen("unix", *socketPath)
	if err != nil {
		logger.Fatal(err)
	}
	mode := os.FileMode(0o600)
	if *group != "" {
		g, err := user.LookupGroup(*group)
		if err != nil {
			logger.Fatalf("group %q: %v", *group, err)
		}
		gid, _ := strconv.Atoi(g.Gid)
		if err := os.Chown(*socketPath, 0, gid); err != nil {
			logger.Fatal(err)
		}
		mode = 0o660
	}
	if err := os.Chmod(*socketPath, mode); err != nil {
		logger.Fatal(err)
	}

	srv := &agent.Server{VHCI: ctl, Version: version, Log: logger}
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGTERM, syscall.SIGINT)
	go func() {
		<-sig
		logger.Printf("stopping: detaching all devices")
		srv.DetachAll()
		l.Close()
	}()
	logger.Printf("listening on %s (mode %o)", *socketPath, mode)
	if err := srv.Serve(l); err != nil {
		logger.Fatal(err)
	}
	srv.Wait() // let every attachment detach its vhci port
	_ = os.Remove(*socketPath)
	logger.Printf("stopped")
}
