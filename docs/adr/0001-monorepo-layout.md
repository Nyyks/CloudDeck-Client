# ADR-0001: Monorepo layout and upstream tracking

- Status: Accepted (layout), Proposed (import mechanics: run in Phase 1)
- Date: 2026-11-04

## Context
The brief asks for one repo containing the moonlight-qt and moonlight-android
forks plus a shared moonlight-common-c, imported with `git subtree` so upstream
changes can be merged later. Upstream uses submodules:

- moonlight-qt: `moonlight-common-c/moonlight-common-c`, `qmdnsengine/qmdnsengine`,
  `app/SDL_GameControllerDB`
- moonlight-android: `app/src/main/jni/moonlight-core/moonlight-common-c`
- moonlight-common-c: `enet` (cgutman fork, **must be this fork**), `nanors`
- Sunshine: 16 submodules

`git subtree` copies the gitlink entries but not a usable `.gitmodules`, so
submodules break unless they're handled explicitly.

## Decision
```
client-desktop/   subtree of moonlight-qt         (tag v6.2.0 at import)
client-android/   subtree of moonlight-android    (tag v12.2 at import)
common/           subtree of moonlight-common-c   (commit pinned by moonlight-qt v6.2.0)
host/agent/       our Go agent (ADR-0002)
host/sunshine-patches/  quilt-style patch series on a pinned Sunshine tag (ADR-0003)
host/install/     install.sh / uninstall.sh / inventory.sh
docs/, .github/workflows/
```
- The clients' own `moonlight-common-c` gitlinks are **removed**. Both build
  files are patched to use `common/` (moonlight-qt
  `moonlight-common-c/moonlight-common-c.pro` and moonlight-android
  `moonlight-core/Android.mk`). The shared protocol code exists only once.
- The remaining nested submodules (`common/enet`, `common/nanors`,
  `client-desktop/qmdnsengine/qmdnsengine`, `client-desktop/app/SDL_GameControllerDB`)
  are registered in the **root** `.gitmodules` at those paths. A later
  `git subtree pull` that bumps one of them just moves the gitlink, which the
  root submodule entry then follows.
- Upstream sync: `scripts/sync-upstream.sh <component> <tag>` runs
  `git subtree pull --prefix=… <upstream> <tag> --squash` on a branch, followed
  by CI.

## Consequences
- Upstream merges show conflicts only where we've diverged. We keep that
  divergence small by putting new code in new files (`*/clouddeck/`) and
  limiting edits to upstream files to hook points.
- Clone with `git clone --recurse-submodules`.
- Sunshine isn't subtree'd, because of its 16 submodules and because we only
  patch it. See ADR-0003.
