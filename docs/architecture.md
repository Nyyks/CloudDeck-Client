# Architecture

```mermaid
flowchart LR
  subgraph Client["Client (Windows / Linux / Android)"]
    UI["CloudDeck view<br/>(QML / Activity)"]
    CS["CloudSession<br/>start-and-connect, auto-stop"]
    CP["CloudProvider<br/>(CloudDeckProvider)"]
    KS[("OS keychain<br/>refresh token")]
    ML["Moonlight core<br/>ComputerManager, pairing, stream"]
    MC["common/<br/>moonlight-common-c"]
    UI --> CS --> CP
    CP --- KS
    CS --> ML --> MC
  end

  subgraph Cloud["CloudDeck"]
    COG["AWS Cognito<br/>(login, refresh)"]
    API["api.clouddeck.app<br/>start / stop / status / PIN"]
    FW{{"Upstream filter<br/>TCP 47984, 47989, 48010<br/>+ Sunshine UDP"}}
  end

  subgraph VM["CloudDeck VM (Bazzite, KDE Plasma)"]
    CTRL["clouddeck-controller<br/>(CloudDeck's agent)"]
    SUN["Sunshine 2026.914<br/>(stock; patched in Phase 2+)"]
    AG["host agent (Go)<br/>Phase 2+, via Sunshine 47984 tunnel"]
  end

  CP -->|HTTPS| COG
  CP -->|"HTTPS, Bearer"| API
  API -.->|"PIN, power"| CTRL
  CTRL -.-> SUN
  MC -->|"RTSP / video / audio / control"| FW --> SUN
  SUN -.->|"/clouddeck/v1/* (ADR-0004)"| AG
```

## Components

- **CloudProvider** (`client-desktop/app/clouddeck/`,
  `client-android/.../clouddeck/`): everything portal-specific, behind one
  interface. It enforces an endpoint allowlist, so `/reset`, billing and
  account changes are impossible.
- **CloudSession / CloudDeckActivity:** UI state and the start-and-connect
  sequence:
  1. machine `running`
  2. DNS points to `public_ip`
  3. Sunshine `serverinfo` answers
  4. pair if needed: client-generated PIN, submitted through the portal
  5. launch through Moonlight's own launcher (`CliStartStream::Launcher` /
     `ServerHelper.doStart`)
- **Moonlight core:** unchanged apart from branding (ADR-0005). Streaming,
  codecs, input and HDR are upstream code.
- **Host side** (Phase 2+): the patched Sunshine (ADR-0003) and the Go agent
  (ADR-0002), reached through Sunshine's HTTPS port because no other port
  gets past the filter (ADR-0004).

## Decisions

See [docs/adr/](adr/).
