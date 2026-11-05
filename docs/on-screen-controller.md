# On-screen controller

Every client has one, for touchscreen devices without a gamepad.

## Android
This is Moonlight's on-screen controller, with CloudDeck additions: Xbox or
PlayStation button style, and named layout profiles. See
docs/controller-profiles.md, section "On-screen controller (Android)".

## Desktop (Windows, Linux: tablets, 2-in-1s, Linux phones)
The owner asked for it in all builds, e.g. for a Linux phone.
- **Turning it on:** in the in-stream menu, switch on *On-screen controller
  (touchscreens)*. It's remembered for the next stream. *PlayStation button
  style* switches the labels and colours between ✕ ○ □ △ / L1 R1 L2 R2 /
  Share Options PS and A B X Y / LB RB LT RT / View Menu Guide.
- **What the CloudDeck sees:** an **Xbox-type controller** with analog
  triggers.
  - In Moonlight's default single-controller mode it is player 1, shared with
    a physical pad.
  - In multi-controller mode it takes the first free slot.
  - After an in-place reconnect it "arrives" again.
  - Hiding it releases everything.
- **Layout:** relative to the stream window, sized in units of 1 % of its
  height:
  - bottom left: left stick and D-pad (8 directions);
  - bottom right: face buttons and right stick;
  - above those: LB/LT and RB/RT;
  - bottom centre: Back, Guide and Start.

  Triggers count as fully pulled while pressed.
- **How it's built:** `clouddeck/onscreencontroller.cpp` and
  `gui/StreamGamepad.qml`.
  - It uses five small windows owned by the stream window (ADR-0009), each
    **masked** to the exact shapes of its controls and 60 % opaque. The stream
    stays visible around them without the transparent swap chains that Qt's
    D3D11 backend doesn't reliably provide.
  - Qt Quick pointer handlers give multi-touch, so a stick and a button work
    at the same time.
  - The windows don't take focus, so the keyboard stays with the stream.
- **Not on desktop:** a layout editor and layout profiles. The layout follows
  the stream window, so it fits any screen.
