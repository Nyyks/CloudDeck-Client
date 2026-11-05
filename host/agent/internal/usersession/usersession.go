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

// User is the desktop user's identity and session environment.
type User struct {
	Name       string
	Home       string
	UID, GID   uint32
	RuntimeDir string
}

// Lookup resolves the desktop user (never root).
func Lookup(username string) (*User, error) {
	u, err := user.Lookup(username)
	if err != nil {
		return nil, err
	}
	uid, err1 := strconv.ParseUint(u.Uid, 10, 32)
	gid, err2 := strconv.ParseUint(u.Gid, 10, 32)
	if err1 != nil || err2 != nil || uid == 0 {
		return nil, fmt.Errorf("unusable desktop user %q", username)
	}
	return &User{Name: u.Username, Home: u.HomeDir, UID: uint32(uid), GID: uint32(gid),
		RuntimeDir: fmt.Sprintf("/run/user/%d", uid)}, nil
}

// Command returns `program args...` set up to run as the user in their session.
// The caller wires stdin/stdout and runs it.
func (u *User) Command(ctx context.Context, program string, args ...string) (*exec.Cmd, error) {
	bin, err := exec.LookPath(program)
	if err != nil {
		return nil, err
	}
	wl, err := waylandSocket(u.RuntimeDir)
	if err != nil {
		return nil, err
	}
	cmd := exec.CommandContext(ctx, bin, args...)
	cmd.Env = []string{
		"HOME=" + u.Home,
		"USER=" + u.Name,
		"LANG=C.UTF-8",
		"PATH=/usr/bin:/bin",
		"XDG_RUNTIME_DIR=" + u.RuntimeDir,
		"WAYLAND_DISPLAY=" + wl,
		"DBUS_SESSION_BUS_ADDRESS=unix:path=" + u.RuntimeDir + "/bus",
		"QT_QPA_PLATFORM=wayland",
	}
	cmd.Dir = "/"
	cmd.SysProcAttr = &syscall.SysProcAttr{Credential: &syscall.Credential{Uid: u.UID, Gid: u.GID}}
	return cmd, nil
}

// Runner returns a function that runs `program` as the desktop user, connected to
// that user's Wayland session and session bus, with a 15 s timeout.
func Runner(username, program string) (func(args ...string) ([]byte, error), error) {
	u, err := Lookup(username)
	if err != nil {
		return nil, err
	}
	if _, err := exec.LookPath(program); err != nil {
		return nil, err
	}
	return func(args ...string) ([]byte, error) {
		ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
		defer cancel()
		cmd, err := u.Command(ctx, program, args...)
		if err != nil {
			return nil, err
		}
		var out, stderr bytes.Buffer
		cmd.Stdout, cmd.Stderr = &out, &stderr
		if err := cmd.Run(); err != nil {
			return nil, WithStderr(err, stderr.String())
		}
		return out.Bytes(), nil
	}, nil
}

// WithStderr adds the last stderr line to err, if there is one.
func WithStderr(err error, stderr string) error {
	if msg := strings.TrimSpace(lastLine(stderr)); msg != "" {
		return fmt.Errorf("%w: %s", err, msg)
	}
	return err
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
