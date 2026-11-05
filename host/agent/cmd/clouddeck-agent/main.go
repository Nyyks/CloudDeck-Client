// clouddeck-agent: CloudDeck-Client host agent (runs as root, systemd system unit).
// Reached only through Sunshine's /clouddeck/v1/* tunnel (patch 0002) over a Unix
// socket. See docs/adr/0006-usb-passthrough.md, 0007-multi-monitor.md and
// 0008-clipboard-sync.md.
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
	"time"

	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/agent"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/clipboard"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/display"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/usersession"
	"github.com/Nyyks/CloudDeck-Client/host/agent/internal/vhci"
)

var version = "dev" // set with -ldflags "-X main.version=..."

func main() {
	socketPath := flag.String("socket", "/run/clouddeck-client/agent.sock", "Unix socket Sunshine connects to")
	group := flag.String("group", "", "group allowed to connect (the desktop user's group, which Sunshine runs as)")
	vhciRoot := flag.String("vhci", vhci.DefaultRoot, "vhci_hcd sysfs directory")
	desktopUser := flag.String("desktop-user", "", "user owning the Plasma session (enables display matching)")
	stateDir := flag.String("state-dir", "/var/lib/clouddeck-client", "where the display mode to restore is remembered")
	flag.Parse()

	logger := log.New(os.Stderr, "", 0) // journald adds timestamps
	logger.Printf("clouddeck-agent %s starting", version)

	ctl := vhci.Controller{Root: *vhciRoot}
	if err := ctl.EnsureLoaded(); err != nil {
		// Keep running: hello still works and the UI can show why USB is unavailable.
		logger.Printf("warning: %v", err)
	}

	srv := &agent.Server{VHCI: ctl, Version: version, Log: logger}
	var disp *display.Manager
	if *desktopUser != "" {
		run, err := usersession.Runner(*desktopUser, "kscreen-doctor")
		if err != nil {
			logger.Printf("warning: display matching disabled: %v", err)
		} else {
			disp = &display.Manager{Run: run, StateFile: filepath.Join(*stateDir, "display-restore.json")}
			srv.Display = disp
			go restoreLeftover(disp, logger)
		}
		busctl, err := usersession.Runner(*desktopUser, "busctl")
		if err != nil {
			logger.Printf("warning: clipboard sync disabled: %v", err)
		} else {
			srv.Clipboard = clipboard.Klipper{Run: busctl}
		}
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

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGTERM, syscall.SIGINT)
	go func() {
		<-sig
		logger.Printf("stopping: detaching all devices, restoring the display")
		srv.DetachAll()
		l.Close()
	}()
	logger.Printf("listening on %s (mode %o)", *socketPath, mode)
	if err := srv.Serve(l); err != nil {
		logger.Fatal(err)
	}
	srv.Wait() // let every attachment detach and every display lease restore
	if disp != nil {
		if err := disp.ReleaseAll(); err != nil {
			logger.Printf("display restore: %v (retried on next start)", err)
		}
	}
	_ = os.Remove(*socketPath)
	logger.Printf("stopped")
}

// restoreLeftover puts back a display mode that a crash or reboot left behind. At
// boot the Plasma session may not exist yet, so it retries for a while.
func restoreLeftover(d *display.Manager, logger *log.Logger) {
	for i := 0; i < 60; i++ {
		restored, err := d.RestoreSaved()
		if err == nil {
			if restored != "" {
				logger.Printf("display: restored %s left over from an interrupted stream", restored)
			}
			return
		}
		if i == 0 {
			logger.Printf("display: leftover mode not restored yet: %v (retrying)", err)
		}
		time.Sleep(10 * time.Second)
	}
	logger.Printf("display: gave up restoring the leftover mode; it is restored when the next stream ends")
}
