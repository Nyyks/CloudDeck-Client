# ADR-0012: Configurable shortcuts and push-to-talk (Phase 5, desktop)

- Status: Accepted (implemented; not yet verified)
- Date: 2026-11-05

## Context
- Moonlight's in-stream shortcuts are fixed as Ctrl+Alt+Shift+<letter>.
  CloudDeck added two more: the menu on O and mic mute on U.
- The brief asks for configurable shortcuts, and Phase 5 includes push-to-talk.

## Decision
1. **Same modifiers, configurable letter.** Every shortcut stays
   Ctrl+Alt+Shift, which no app on the CloudDeck is likely to need. Only the
   letter or digit changes, so Moonlight's combo matching (key code first, then
   scan code for non-Latin layouts) keeps working unchanged.
   - The `Shortcuts` QML singleton stores the key per action under
     `clouddeck/shortcuts/<id>`.
   - `SdlInputHandler` overrides its table from it when a stream starts.
   - The menu hotkey reads it too.
   - Conflicts are rejected with the name of the action that already uses the
     key. There's a *Reset to defaults* button.
2. **Push-to-talk:**
   - A choice of keys that rarely matter on the remote side: Caps Lock,
     backtick, Right Alt, Right Ctrl, Scroll Lock, Pause, F13. Or a mouse side
     button (X1/X2).
   - While it's set and the mic is on, `MicStreamer` only encodes and sends
     while the binding is held. The mute switch still overrides.
   - The SDL loop consumes the key or button before Moonlight's input
     handling, so the CloudDeck never sees it.
   - Losing focus counts as releasing it, so it can't get stuck "on".
3. **UI:**
   - CloudDeck screen: *Shortcuts during a stream* (a collapsible list of
     actions with a key picker each) and *Push-to-talk*.
   - The in-stream menu shows the push-to-talk hint, and its texts follow the
     configured menu key.

## Consequences
- Caps Lock as push-to-talk still toggles the local Caps Lock state. Only the
  CloudDeck doesn't see it.
- Changes apply when the next stream starts.
- Android push-to-talk and a mic mute button are separate (Android UI).
