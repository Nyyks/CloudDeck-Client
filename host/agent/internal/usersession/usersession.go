// Package usersession runs commands as the desktop user inside their Plasma session
// (Wayland socket and session D-Bus); the agent itself runs as root.
package usersession

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"
)

// Runner returns a function that runs `program` as the desktop user, connected to
// that user's Wayland session and session bus, with a 15 s timeout.
func Runner(username, program string) (func(args ...string) ([]byte, error), error) {
	u, err := user.Lookup(username)
	if err != nil {
		return nil, err
	}
	uid, err1 := strconv.ParseUint(u.Uid, 10, 32)
	gid, err2 := strconv.ParseUint(u.Gid, 10, 32)
	if err1 != nil || err2 != nil || uid == 0 {
		return nil, fmt.Errorf("unusable desktop user %q", username)
	}
	bin, err := exec.LookPath(program)
	if err != nil {
		return nil, err
	}
	runtimeDir := fmt.Sprintf("/run/user/%d", uid)
	return func(args ...string) ([]byte, error) {
		wl, err := waylandSocket(runtimeDir)
		if err != nil {
			return nil, err
		}
		ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
		defer cancel()
		cmd := exec.CommandContext(ctx, bin, args...)
		cmd.Env = []string{
			"HOME=" + u.HomeDir,
			"USER=" + u.Username,
			"LANG=C.UTF-8",
			"PATH=/usr/bin:/bin",
			"XDG_RUNTIME_DIR=" + runtimeDir,
			"WAYLAND_DISPLAY=" + wl,
			"DBUS_SESSION_BUS_ADDRESS=unix:path=" + runtimeDir + "/bus",
			"QT_QPA_PLATFORM=wayland",
		}
		cmd.Dir = "/"
		cmd.SysProcAttr = &syscall.SysProcAttr{Credential: &syscall.Credential{Uid: uint32(uid), Gid: uint32(gid)}}
		var out, stderr bytes.Buffer
		cmd.Stdout, cmd.Stderr = &out, &stderr
		if err := cmd.Run(); err != nil {
			msg := strings.TrimSpace(lastLine(stderr.String()))
			if msg != "" {
				return nil, fmt.Errorf("%w: %s", err, msg)
			}
			return nil, err
		}
		return out.Bytes(), nil
	}, nil
}

// waylandSocket finds the compositor socket (normally wayland-0).
func waylandSocket(runtimeDir string) (string, error) {
	if st, err := os.Stat(filepath.Join(runtimeDir, "wayland-0")); err == nil && st.Mode()&os.ModeSocket != 0 {
		return "wayland-0", nil
	}
	matches, _ := filepath.Glob(filepath.Join(runtimeDir, "wayland-[0-9]"))
	for _, m := range matches {
		if st, err := os.Stat(m); err == nil && st.Mode()&os.ModeSocket != 0 {
			return filepath.Base(m), nil
		}
	}
	return "", errors.New("no Wayland session (is the desktop user logged in?)")
}

func lastLine(s string) string {
	s = strings.TrimSpace(s)
	if i := strings.LastIndexByte(s, '\n'); i >= 0 {
		return s[i+1:]
	}
	return s
}
