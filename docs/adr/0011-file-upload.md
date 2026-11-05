# ADR-0011: File upload to the CloudDeck (Phase 5, desktop)

- Status: Accepted (implemented; not yet verified)
- Date: 2026-11-05

## Context
The brief asks for drag-and-drop or upload to the host via the agent. The agent
runs as root in a systemd sandbox where `/home` is read-only (`ProtectHome=read-only`,
ADR-0007). Writing into the user's home as root would also invite symlink
tricks.

## Decision
1. **Protocol:** `GET /clouddeck/v1/file/upload?name=<percent-encoded>&size=<bytes>`
   upgrades to a tunnel (ADR-0004/0006 transport, paired clients only). After
   `OK`, the client sends exactly `size` bytes. The agent answers one JSON line,
   `{"saved":"<path>"}` or `{"error":"…"}`. There's one tunnel per file.
2. **Writing as the user:**
   - The agent starts `systemd-run --user --pipe --wait -- /bin/sh -c <script>`
     as the desktop user (`usersession.User.Command`). The process lives in the
     user's own service manager, outside the agent's sandbox, with the user's
     normal permissions.
   - The tunnel bytes, at most `size`, stream into its stdin.
   - The script writes `~/Downloads/CloudDeck/<name>.part` (the XDG download
     folder if set). It checks that exactly `size` bytes arrived, then renames
     the file. An existing name gets ` (1)`, ` (2)`, …
   - An interrupted upload removes the `.part` file, so nothing half-written
     ever looks complete.
3. **Names:** the agent keeps only the base name and drops control characters
   and leading dots (no hidden files, `.` or `..`). It caps the name at 200
   bytes, keeping the extension.
4. **Client:**
   - Files dropped onto the stream window (`SDL_DROPFILE`) or picked with
     *Upload files to my CloudDeck…* in the in-stream menu (Qt Quick
     `FileDialog`) go into one queue.
   - A worker thread sends the files one after another, keeping at most about
     2 MiB in flight.
   - Progress ("Uploading a.pdf (2 of 3): 45%") and the result show in the
     notice under the menu button; the result hides after 5 s.
   - Folders are skipped with a hint to zip them first.
5. **Limits:** 64 GiB per file. In practice the VM's disk and the uplink are
   the limits.

## Consequences
- Each upload is a transient user service (`run-u….service`, `--collect`), so
  `journalctl --user` shows it.
- Dragging needs a free cursor: windowed or desktop mouse mode, so you can drag
  from Explorer onto the stream.
- Downloads from the CloudDeck to the PC aren't part of this (not in the brief).

## Tests
`host/agent/internal/upload`: the real script under `sh` writes the file,
numbers duplicates (with and without an extension), leaves nothing after a short
upload, and reads no more than `size`. Name sanitizing and the `systemd-run`
arguments are covered too. `internal/agent`: the server operation and its
rejections. The client QML loads in `tests/qml`.
