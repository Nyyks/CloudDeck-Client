# CloudDeck portal API (reverse-engineered)

Status: **draft from static analysis**. Everything here comes from the public
JavaScript bundle of `https://portal.clouddeck.app` (Angular SPA,
`main.82efa2b20cc44d86.js`, fetched 2026-11-04). Nothing has been verified
against a live account yet. Rows marked *unverified* need a confirming request
(Phase 0, step 2b, below).

There is no public or documented API. The portal may change at any time, so all
of this sits behind the `CloudProvider` interface (see the end of this file).

## Hosts

| Purpose | Host | Notes |
|---|---|---|
| REST API | `https://api.clouddeck.app` | CloudFront (`d3526f848c694h.cloudfront.net`) |
| Auth | `https://cognito-idp.eu-central-1.amazonaws.com/` | AWS Cognito user pool |
| Moonlight / Sunshine host | `<machine.id minus first 2 chars>.my.clouddeck.app` | `*.my.clouddeck.app` is a Cloudflare-proxied wildcard for unknown names. The real VM record must be DNS-only, because Sunshine needs UDP. *Unverified.* |
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
- **Refresh:** `InitiateAuth` with `AuthFlow=REFRESH_TOKEN_AUTH`,
  `AuthParameters={REFRESH_TOKEN}`. Cognito's default refresh-token lifetime is
  30 days (pool-configurable). *Lifetime unverified.* The client keeps **only
  the refresh token** in the OS keychain and never the password.
- **MFA / 2FA:** the bundle contains Amplify's generic `SMS_MFA` /
  `SOFTWARE_TOKEN_MFA` handling, but no portal UI for setting up MFA. Assume it's
  off, but handle a `ChallengeName` response by prompting for a code
  (`RespondToAuthChallenge`).
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
| GET | `/machines/{machineId}` | – | Machine: `id, status, os, region, last_started, created_at, password` | Yes (status polling) |
| POST | `/machines/{id}/start` | `{session?: "desktop"\|"gaming", keyboard_layout?: "us"\|"de"\|"fr"}` | Start VM | Yes |
| POST | `/machines/{id}/stop` | `{}` | Stop VM | Yes |
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
firewall allows only that IP. Unknowns, to resolve in Phase 0:
- **Where is the filter?** On the VM (firewalld/nftables, which `inventory.sh`
  checks) or upstream (provider security group)? If it's upstream, we can't
  change it, and **every port the agent or extra Sunshine instances need must be
  one that's already forwarded**.
- **Which ports are forwarded?** Probably the Sunshine set: TCP
  47984/47989/47990/48010 and UDP 47998–48000/48010. Test from the client while
  the VM is running.
- **IPv6?** Not checked yet.
- **Roaming:** a phone switching from Wi-Fi to mobile data changes its IP.
  Mitigation: the client detects the change (Sunshine unreachable plus public IP
  changed) and offers "re-authorize": calling `start` on a running machine may
  re-register the IP (*unverified*), otherwise stop and start.

## Phase 0, step 2b: live verification plan

Read-only first, and only with your go-ahead:
1. Log in through Cognito with credentials from an untracked `.env`. Record the
   token lifetimes and `custom:account`.
2. `GET /accounts/{id}` and `GET /machines/{id}`. Record the JSON shapes, with
   values redacted.
3. Then, with your permission (starting the VM may count against usage):
   `POST /start` and time the transitions, check the DNS record of
   `<id>.my.clouddeck.app`, probe the reachable ports, and check the firewall on
   the VM before and after the start.
4. Try a second public IP (e.g. phone hotspot) to confirm the allowlist.

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
