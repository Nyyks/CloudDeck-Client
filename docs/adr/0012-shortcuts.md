# ADR-0012: Configurable shortcuts (Phase 5, desktop)

- Status: Accepted (implemented; not yet verified)
- Date: 2026-11-05
- Revision: a first version also had push-to-talk. The owner dropped it ("better
  handled inside the VM, in the games themselves") and it was removed.

## Context
- Moonlight's in-stream shortcuts are fixed as Ctrl+Alt+Shift+<letter>.
  CloudDeck added two more: the menu on O and mic mute on U.
- The brief asks for configurable shortcuts. The owner keeps them for anyone who
  wants different keys; the defaults stay the standard ones.

## Decision
- **Same modifiers, configurable letter.** Every shortcut stays
  Ctrl+Alt+Shift, which no app on the CloudDeck is likely to need. Only the
  letter or digit changes, so Moonlight's combo matching (key code first, then
  scan code for non-Latin layouts) keeps working unchanged.
- The `Shortcuts` QML singleton stores the key per action under
  `clouddeck/shortcuts/<id>`. `SdlInputHandler` overrides its table from it when
  a stream starts, and the menu hotkey and its hints follow it too.
- Conflicts are rejected with the name of the action that already uses the
  key. There's a *Reset to defaults* button.
- UI: *Shortcuts during a stream → Change…* on the CloudDeck screen.

## Consequences
Changes apply when the next stream starts.
