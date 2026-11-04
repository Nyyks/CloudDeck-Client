# CloudDeck portal API (reverse-engineered)

Status: from static analysis of the public JavaScript bundle of
`https://portal.clouddeck.app` (Angular SPA, `main.82efa2b20cc44d86.js`, fetched
2026-11-04), **verified live on 2026-11-04** for login, refresh, account,
machine, start, and stop. Anything still marked *unverified* hasn't been
exercised yet.

There is no public or documented API. The portal may change at any time, so all
of this sits behind the `CloudProvider` interface (see the end of this file).

## Hosts

| Purpose | Host | Notes |
|---|---|---|
| REST API | `https://api.clouddeck.app` | CloudFront (`d3526f848c694h.cloudfront.net`) |
| Auth | `https://cognito-idp.eu-central-1.amazonaws.com/` | AWS Cognito user pool |
| Moonlight / Sunshine host | `<machine.id minus first 2 chars>.my.clouddeck.app` | **Verified.** A DNS-only A record. While `running` it points to the VM's public IP (= `machine.public_ip`). While `off` it points to `52.29.233.180` (AWS, parking/wake). Unknown names hit a Cloudflare wildcard. The client must not cache DNS across a start. |
| Browser streaming (WebRTC) | `wss://<machine.id>.cldk.me/ws/signal`, `https://<id>.cldk.me/health` | Feature flag `browserStreaming: false` in production. Implies a CloudDeck-owned streaming agent may exist on the VM; Phase 0 must identify it and leave it alone. |
| Status page | `https://status.clouddeck.app/index.json` | Public |

## Authentication

From the bundle's environment config:

```
cognito: { userPoolId: "eu-central-1_DQtJnTkIf",
           userPoolWebClientId: "2e7an7pt3vqdae0abgskfs38k8",
           authenticationFlowType: "USER_PASSWORD_AUTH" }
```

These identifiers are public (shipped to every browser), not secrets.

- **Login:** Cognito `InitiateAuth` with `AuthFlow=USER_PASSWORD_AUTH`,
  `ClientId=2e7an7pt3vqdae0abgskfs38k8`, `AuthParameters={USERNAME, PASSWORD}`.
  This is a plain JSON POST with header
  `X-Amz-Target: AWSCognitoIdentityProviderService.InitiateAuth` and
  `Content-Type: application/x-amz-json-1.1`. No AWS SDK or SigV4 needed; it's
  an unauthenticated public-client call. Returns `IdToken`, `AccessToken`, and
  `RefreshToken`.
- **API auth:** an HTTP interceptor adds `Authorization: Bearer <AccessToken>` to
  every request to `api.clouddeck.app`, except `POST /accounts` (signup).
- **Token lifetimes (verified):** access and ID tokens last 3600 s.
- **Refresh (verified):** `InitiateAuth` with `AuthFlow=REFRESH_TOKEN_AUTH`,
  `AuthParameters={REFRESH_TOKEN}` returns new access and ID tokens. **The
  refresh token is not rotated** (none is returned). Its lifetime is
  pool-configured; Cognito's default is 30 days. *Lifetime unverified*: the
  client re-prompts for the password when refresh fails with
  `NotAuthorizedException`. The client keeps **only the refresh token** in the
  OS keychain and never the password.
- **MFA / 2FA (verified for this account):** login returned tokens directly,
  with no challenge. The bundle contains Amplify's generic `SMS_MFA` /
  `SOFTWARE_TOKEN_MFA` handling, so the client still handles a `ChallengeName`
  response by prompting for a code (`RespondToAuthChallenge`).
- **Unauthenticated API calls (verified):** return HTTP 401.
- **Account ID:** comes from the Cognito user attribute `custom:account`
  (`currentAuthenticatedUser().attributes`). It's also present as a claim in the
  **ID** token, or available from Cognito `GetUser` with the access token.

## Endpoints

All are relative to `https://api.clouddeck.app` and need the Bearer token.

| Method | Path | Body | Used for | Client use |
|---|---|---|---|---|
| GET | `/accounts/{accountId}` | – | Account; contains `machine_id` | Yes |
| PATCH | `/accounts/{accountId}` | partial | Account settings | **No** (brief: don't change account settings) |
| DELETE | `/accounts/{accountId}` | – | Delete account | **Never** |
| GET | `/machines/{machineId}` | – | Machine: `id, status, os, region, public_ip, last_started, created_at, password` (verified). `password` is the VM desktop user's password: never log or display it. | Yes (status polling) |
| POST | `/machines/{id}/start` | `{session?: "desktop"\|"gaming", keyboard_layout?: "us"\|"de"\|"fr"}` | Start VM. `{}` → **204** (verified). | Yes |
| POST | `/machines/{id}/stop` | `{}` | Stop VM → **204** (verified) | Yes |
| POST | `/machines/{id}/clients` | `{pin: "1234"}` | **Submit Moonlight pairing PIN** | Yes (auto-pair) |
| POST | `/machines/{id}/clients/reset` | `{}` | Unpair all clients | Settings only, behind a confirmation |
| POST | `/machines/{id}/reset` | `{}` | **Factory reset: "This will erase all your data and cannot be undone."** | **Never.** Not exposed. |
| POST | `/machines/{id}/stream-token` | `{}` | Token for browser (WebRTC) streaming | Not needed |
| POST | `/machines/{id}/storage-upgrades` | … | Billing | **No** (costs money) |
| GET | `/datacenters`, `/maintenance-windows?datacenter=` | – | Region list, upcoming maintenance | Maybe (show a maintenance warning) |
| POST | `/checkout-sessions`, `/billing-portal-sessions`, `/reservations`, `/availability-checks`, `/checkout-discount`; GET/… `/subscriptions/{id}` | … | Billing / signup | **No** |

The client's HTTP layer must hard-block the "Never"/"No" paths with an allowlist,
not a denylist.

### Machine status values

Seen in the bundle: `off`, `starting`, `running`, `stopping`, `pending`,
`provisioning`, `ready`, `error`, `failed`. The portal treats `running` and `off`
as terminal states for start/stop.

### Observed timings (2026-11-04, eu-de-nbg-1)

| Transition | Time |
|---|---|
| `POST /start` → `starting` | immediate |
| `starting` → `running` (`public_ip` set, DNS repointed) | ≈ 80 s |
| `running` → Sunshine `serverinfo` reachable on 47989 | already reachable at the first probe after `running` |
| `POST /stop` → `stopping` | ≈ 7 s |
| `stopping` → `off` (`public_ip` cleared, DNS back to parking) | ≈ 75 s |

`last_started` is set to the time of the start request (millisecond epoch,
rounded to 100 s steps).

### Start semantics (from portal code)

1. On the first start of an image whose `os` starts with `clouddeck-26.`, the
   portal asks for a **session mode** (`desktop` = "Desktop Mode", `gaming` =
   Game Mode) and sends the browser's detected keyboard layout. Otherwise it
   sends `{}`.
2. After `POST /start` the portal waits 3 s, then polls `GET /machines/{id}`
   every 10 s, up to 18 times (about 3 min), until `status ∈ {running, off}`.
   Ending on `off` counts as failure.
3. There is **no restart endpoint**. "Restart" in our client means `stop`, then
   poll for `off`, then `start`.

**Important for multi-monitor:** `session: "desktop"` vs `"gaming"` is probably
how the gamescope-vs-Plasma choice reaches the VM. *Unverified*: it may only take
effect on the first boot.

### Pairing

The portal's "Connect" dialog:
1. Shows the address `machine.id.slice(2) + ".my.clouddeck.app"`.
2. You start pairing in Moonlight, which displays a 4-digit PIN.
3. You type the PIN into the portal, which sends `POST /machines/{id}/clients {pin}`.
   The backend presumably forwards it to Sunshine's `/api/pin` on the VM.

The Moonlight client generates the PIN itself (moonlight-common-c `pairing`), so
automatic pairing is simple: generate the PIN, start the `/pair` handshake with
Sunshine, and in parallel `POST /clients {pin}`. Sunshine holds the pairing
request open until the PIN arrives. Fallback if the portal path breaks: the host
agent accepts the PIN over its own authenticated channel (it can call Sunshine's
local `/api/pin`). That only works after the agent is installed and the client
has another way to authenticate, so the portal path stays primary.

### IP allowlisting

The portal's troubleshooting text says: *"If you start your CloudDeck from one
device and connect with Moonlight from another, both devices must be on the
same network. Restart your CloudDeck after confirming."*

This strongly suggests that `POST /start` records the caller's public IP and a
firewall allows only that IP.

**Verified (2026-11-04):**
- **The filter is upstream, not on the VM.** The VM's firewalld opens TCP and
  UDP 1025–65535, yet from the internet:
- **Only TCP 47984, 47989 and 48010 are reachable** (Sunshine HTTPS, HTTP, and
  RTSP). Port 22 (SSH), 47990 (web UI), every other port in 47970–48030, and
  80, 443, 3240, 8080, 8443, 8989, 49000 and 50123 all time out. UDP couldn't be
  probed this way; streaming works, so UDP 47998–48000 are open.
- Consequence: **nothing we add on the host can get its own inbound port.** See
  ADR-0004 (tunnel through 47984).

**Still unverified:**
- Whether the filter is per-IP: test from a second network.
- Whether IPv6 is filtered the same way. The VM has a public IPv6, but the DNS
  record is A-only.
- **Roaming:** a phone switching from Wi-Fi to mobile data changes its IP.
  Mitigation: the client detects the change (Sunshine unreachable plus public IP
  changed) and offers "re-authorize": calling `start` on a running machine may
  re-register the IP (*unverified*), otherwise stop and start.

## Response shapes (verified, values redacted)

```
GET /accounts/{a_…}  { id, machine_id, region, email, created_at, customer_id,
                       subscription_id, user_id, marketing_consent }
GET /machines/{m_…}  { id, status, region, os, public_ip, last_started,
                       created_at, password }
GET /datacenters     [ { id: "d_…", region: "eu-de-nbg-1", capacities: { base } }, … ]
```
IDs are prefixed: `a_` account, `m_` machine, `d_` datacenter. The host name
drops the `m_` prefix.

## Remaining verification
- `POST /clients {pin}` during a real Moonlight pairing (Phase 1, with our client).
- The allowlist from a second public IP (e.g. a phone hotspot).

## CloudProvider interface (client side)

The desktop client is C++/Qt and the Android client is Java, so there are two
implementations. They share this spec and a set of JSON fixtures
(`common/cloudprovider-fixtures/`, planned) that both test suites replay.

```
interface CloudProvider {
  signIn(email, password) -> Session | MfaChallenge
  respondMfa(challenge, code) -> Session
  restore(refreshToken) -> Session          // on app start
  signOut()
  listMachines() -> [Machine]               // today: account.machine_id -> [1]
  getMachine(id) -> Machine                 // {id, status, os, region, lastStarted}
  start(id, opts{sessionMode?, keyboardLayout?})
  stop(id)
  restart(id)                               // stop -> wait off -> start
  hostAddress(machine) -> String            // "<id[2:]>.my.clouddeck.app"
  submitPairingPin(id, pin)
  // no reset(), no billing, no account mutation
}
```
