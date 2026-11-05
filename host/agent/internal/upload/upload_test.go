package upload

import (
	"bytes"
	"context"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

// directSh runs the script with plain sh and HOME pointing at a temp dir (no
// xdg-user-dir there, so it falls back to ~/Downloads).
func directSh(home string) Launcher {
	return func(ctx context.Context, script string, args ...string) (*exec.Cmd, error) {
		cmd := exec.CommandContext(ctx, "/bin/sh", append([]string{"-c", script, "clouddeck-upload"}, args...)...)
		cmd.Env = []string{"HOME=" + home, "PATH=/usr/bin:/bin"}
		return cmd, nil
	}
}

func TestSanitizeName(t *testing.T) {
	for in, want := range map[string]string{
		"report.pdf":             "report.pdf",
		"C:\\Users\\me\\a b.txt": "a b.txt",
		"/etc/passwd":            "passwd",
		"../../.ssh/id_rsa":      "id_rsa",
		".bashrc":                "bashrc",
		"we\x00ird\nname.txt":    "weirdname.txt",
		"Bild äöü ✓.png":         "Bild äöü ✓.png",
	} {
		got, err := SanitizeName(in)
		if err != nil || got != want {
			t.Errorf("SanitizeName(%q) = %q, %v; want %q", in, got, err, want)
		}
	}
	for _, bad := range []string{"", ".", "..", "/", "...", "\n"} {
		if _, err := SanitizeName(bad); err == nil {
			t.Errorf("accepted %q", bad)
		}
	}
	long, _ := SanitizeName(strings.Repeat("x", 300) + ".tar.gz")
	if len(long) > 200 || !strings.HasSuffix(long, ".gz") {
		t.Errorf("long name = %q (%d)", long, len(long))
	}
}

func TestSaveWritesAndNumbersDuplicates(t *testing.T) {
	home := t.TempDir()
	s := Saver{Launch: directSh(home)}
	data := []byte("hello CloudDeck\x00\xff binary")
	p1, err := s.Save(context.Background(), "a.txt", int64(len(data)), bytes.NewReader(data))
	if err != nil {
		t.Fatal(err)
	}
	if want := filepath.Join(home, "Downloads", "CloudDeck", "a.txt"); p1 != want {
		t.Fatalf("saved to %q, want %q", p1, want)
	}
	if b, _ := os.ReadFile(p1); !bytes.Equal(b, data) {
		t.Fatalf("content = %q", b)
	}
	p2, err := s.Save(context.Background(), "a.txt", 3, strings.NewReader("abc"))
	if err != nil || filepath.Base(p2) != "a (1).txt" {
		t.Fatalf("duplicate saved as %q, %v", p2, err)
	}
	p3, err := s.Save(context.Background(), "Makefile", 1, strings.NewReader("x"))
	if err != nil || filepath.Base(p3) != "Makefile" {
		t.Fatalf("no-extension file: %q, %v", p3, err)
	}
	p4, _ := s.Save(context.Background(), "Makefile", 1, strings.NewReader("y"))
	if filepath.Base(p4) != "Makefile (1)" {
		t.Fatalf("no-extension duplicate: %q", p4)
	}
}

func TestShortUploadLeavesNothing(t *testing.T) {
	home := t.TempDir()
	s := Saver{Launch: directSh(home)}
	_, err := s.Save(context.Background(), "big.bin", 100, strings.NewReader("only 12 byte"))
	if err == nil || !strings.Contains(err.Error(), "12 of 100") {
		t.Fatalf("err = %v", err)
	}
	entries, _ := os.ReadDir(filepath.Join(home, "Downloads", "CloudDeck"))
	if len(entries) != 0 {
		t.Fatalf("left behind: %v", entries)
	}
}

func TestSaveReadsOnlySizeBytes(t *testing.T) {
	home := t.TempDir()
	s := Saver{Launch: directSh(home)}
	r := strings.NewReader("12345trailing")
	p, err := s.Save(context.Background(), "n.txt", 5, r)
	if err != nil {
		t.Fatal(err)
	}
	if b, _ := os.ReadFile(p); string(b) != "12345" {
		t.Fatalf("content = %q", b)
	}
	if rest := r.Len(); rest != len("trailing") {
		t.Fatalf("consumed past the size: %d left", rest)
	}
	if _, err := s.Save(context.Background(), "x", -1, r); err == nil {
		t.Fatal("negative size accepted")
	}
}

func TestSystemdRunArgs(t *testing.T) {
	var got []string
	l := SystemdRun(func(ctx context.Context, program string, args ...string) (*exec.Cmd, error) {
		got = append([]string{program}, args...)
		return exec.Command("true"), nil
	})
	if _, err := l(context.Background(), "SCRIPT", "a.txt", "3"); err != nil {
		t.Fatal(err)
	}
	s := strings.Join(got, " ")
	if !strings.HasPrefix(s, "systemd-run --user --pipe") || !strings.HasSuffix(s, "-- /bin/sh -c SCRIPT clouddeck-upload a.txt 3") {
		t.Fatalf("args: %s", s)
	}
}
