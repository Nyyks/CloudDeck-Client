# CloudDeck-Client

A Shadow-class desktop and mobile client for a personal [CloudDeck](https://clouddeck.app)
cloud PC (Bazzite plus Sunshine). It extends the Moonlight ecosystem: one-click
start-and-connect, automatic pairing, and (planned) microphone passthrough, USB
passthrough, and multiple monitors.

Unofficial; not affiliated with CloudDeck, Moonlight, or LizardByte.

## Status

**Phase 1 (MVP) in progress.** CI builds Windows, Linux and Android clients; the CloudDeck sign-in and start-and-connect flow is implemented but not yet verified end-to-end. See
[docs/STATUS.md](docs/STATUS.md) and [docs/feasibility.md](docs/feasibility.md).

## Layout

| Path | What |
|---|---|
| `client-desktop/` | moonlight-qt fork (Windows / Linux) *(Phase 1)* |
| `client-android/` | moonlight-android fork *(Phase 1)* |
| `common/` | moonlight-common-c fork shared by both clients *(Phase 1)* |
| `host/agent/` | companion host agent (Go) |
| `host/sunshine-patches/` | patch series on upstream Sunshine |
| `host/install/` | Bazzite setup scripts (`inventory.sh` is read-only) |
| `docs/` | design docs, ADRs, the original [brief](docs/BRIEF.md) |

## Development setup

```sh
git clone --recurse-submodules <repo>
git config core.hooksPath .githooks   # gitleaks pre-commit hook (requires gitleaks)
cp .env.example .env                  # local secrets, never committed
```

## License

GPL-3.0-only, the same as the upstream projects it's built on. See [LICENSE](LICENSE)
and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
