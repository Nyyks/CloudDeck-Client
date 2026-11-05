// Package upload saves files sent by a client into the desktop user's
// Downloads/CloudDeck folder (Phase 5, docs/adr/0011-file-upload.md).
//
// The agent runs as root in a sandbox where /home is read-only, so the file is
// written by a process in the user's own systemd manager (systemd-run --user --pipe),
// with the user's normal permissions. The bytes stream from the tunnel into that
// process. The file only gets its final name once exactly the announced size
// arrived; an interrupted upload leaves nothing behind.
package upload

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os/exec"
	"path"
	"strings"
	"unicode"
	"unicode/utf8"
)

// MaxSize caps one upload (64 GiB); the VM disk is the real limit.
const MaxSize = 64 << 30

// Script runs as the user: $1 = file name (already sanitized), $2 = expected size.
// It prints the saved path. A name that exists gets " (1)", " (2)", ... appended.
const Script = `set -eu
name="$1"; size="$2"
dir="$(xdg-user-dir DOWNLOAD 2>/dev/null || true)"
if [ -z "$dir" ] || [ "$dir" = "$HOME" ]; then dir="$HOME/Downloads"; fi
dir="$dir/CloudDeck"
mkdir -p "$dir"
base="$name"; ext=""
case "$name" in ?*.*) base="${name%.*}"; ext=".${name##*.}" ;; esac
f="$dir/$name"; n=1
while [ -e "$f" ] || [ -e "$f.part" ]; do f="$dir/$base ($n)$ext"; n=$((n+1)); done
tmp="$f.part"
trap 'rm -f "$tmp"' EXIT
cat > "$tmp"
got="$(stat -c %s "$tmp")"
if [ "$got" != "$size" ]; then echo "incomplete upload: $got of $size bytes" >&2; exit 3; fi
mv "$tmp" "$f"
trap - EXIT
printf '%s\n' "$f"
`

// SanitizeName turns a client-supplied name into a safe single file name.
func SanitizeName(name string) (string, error) {
	name = strings.ReplaceAll(name, "\\", "/")
	name = path.Base(name)
	var b strings.Builder
	for _, r := range name {
		if r == utf8.RuneError || unicode.IsControl(r) || r == '/' {
			continue
		}
		b.WriteRune(r)
	}
	name = strings.TrimSpace(b.String())
	name = strings.TrimLeft(name, ".") // no hidden files, no "." or ".."
	if name == "" {
		return "", errors.New("invalid file name")
	}
	for len(name) > 200 { // bytes; keep the extension
		ext := path.Ext(name)
		if len(ext) > 20 {
			ext = ""
		}
		r := []rune(strings.TrimSuffix(name, ext))
		name = string(r[:len(r)-1]) + ext
	}
	return name, nil
}

// Launcher starts the script: given the script and its arguments, it returns the
// command to run (as the user, through systemd-run, in production).
type Launcher func(ctx context.Context, script string, args ...string) (*exec.Cmd, error)

// Saver writes uploads.
type Saver struct {
	Launch Launcher
}

// Save streams exactly size bytes from r into the user's folder and returns the
// saved path. It reads at most size bytes from r.
func (s Saver) Save(ctx context.Context, name string, size int64, r io.Reader) (string, error) {
	if size < 0 || size > MaxSize {
		return "", fmt.Errorf("size %d out of range", size)
	}
	cmd, err := s.Launch(ctx, Script, name, fmt.Sprint(size))
	if err != nil {
		return "", err
	}
	stdin, err := cmd.StdinPipe()
	if err != nil {
		return "", err
	}
	var out, stderr strings.Builder
	cmd.Stdout, cmd.Stderr = &out, &stderr
	if err := cmd.Start(); err != nil {
		return "", err
	}
	n, copyErr := io.Copy(stdin, io.LimitReader(r, size))
	stdin.Close()
	waitErr := cmd.Wait()
	if copyErr == nil && n < size {
		copyErr = fmt.Errorf("connection closed after %d of %d bytes", n, size)
	}
	if copyErr != nil {
		return "", copyErr
	}
	if waitErr != nil {
		msg := strings.TrimSpace(stderr.String())
		if msg != "" {
			return "", fmt.Errorf("%w: %s", waitErr, msg)
		}
		return "", waitErr
	}
	return strings.TrimSpace(out.String()), nil
}

// SystemdRun launches through `systemd-run --user --pipe`, i.e. outside the
// agent's sandbox, in the user's own service manager. userCmd builds a command
// that runs as the desktop user (usersession.User.Command).
func SystemdRun(userCmd func(ctx context.Context, program string, args ...string) (*exec.Cmd, error)) Launcher {
	return func(ctx context.Context, script string, args ...string) (*exec.Cmd, error) {
		a := []string{"--user", "--pipe", "--quiet", "--collect", "--wait", "--unit-description=CloudDeck file upload",
			"--", "/bin/sh", "-c", script, "clouddeck-upload"}
		return userCmd(ctx, "systemd-run", append(a, args...)...)
	}
}
