# Protocol extensions

Every extension is negotiated. A stock client talking to our patched Sunshine,
or our client talking to stock Sunshine, behaves exactly like stock Moonlight.

## 1. Microphone stream, v1 (`x-ss-mic`)

Client → host Opus audio, encrypted with the session key and carried on the
**existing audio UDP port**. No new port is added: CloudDeck's upstream filter
only forwards Sunshine's ports (feasibility §1), and Sunshine maintainers asked
for no new ports (PR #4078 review).

### 1.1 Negotiation (RTSP)

| Step | Direction | Content |
|---|---|---|
| DESCRIBE response | host → client | SDP line `a=x-ss-mic.version:1` if the host supports mic v1 |
| SETUP `streamid=mic/0/0` | client → host | Only when the host advertised v1 **and** the user enabled the mic. Response: `Transport: server_port=<audio port>` and `X-SS-Ping-Payload: <16 hex chars>` (the session's audio ping payload, i.e. the session tag) |
| ANNOUNCE | client → host | SDP attribute `a=x-ss-mic.enabled:1` |

The host only accepts mic packets for sessions whose ANNOUNCE carried
`x-ss-mic.enabled:1`. A stock host never advertises v1, so the client never
sends SETUP/ANNOUNCE for it. A stock client never sends them, so the host never
creates a mic for it.

We deliberately do **not** advertise `a=rtpmap:96 opus/48000/1`, the attribute
used by the logabell / Vibepollo mic forks. Their clients would then try their
own, incompatible transport (separate port, AES-CBC).

### 1.2 Packet format

UDP datagram from the client's existing audio socket to the host's audio port.
All integers are big-endian.

```
offset size field                                   (clear text)
0      1    type       = 0x8D  (non-ASCII: can never start a hex ping payload, so
                                stock Sunshine ignores the packet)
1      1    version    = 1
2      2    reserved   = 0
4      16   sessionTag = the X-SS-Ping-Payload of the session (routes the packet,
                         exactly like ping packets do; needed for multiple clients)
20     8    seq        monotonically increasing per session, starts at 0
28     16   GCM tag
44     n    ciphertext of:
              u16 flags   bit0 = packet is a FEC duplicate; others 0
              u32 rtpTs   48 kHz sample timestamp of the first sample in the frame
              ...         one Opus packet (n - 6 bytes)
```

Minimum size 51 bytes (1-byte Opus packet). Maximum Opus payload 1200 bytes.

### 1.3 Crypto

- Cipher: **AES-128-GCM** with the session's launch key (`rikey`, the key the
  control and video streams already use). Both sides use their existing GCM
  helpers, so neither crypto layer needs patching.
- Nonce: 12 bytes. Bytes 0–7 = `seq` as little-endian uint64, bytes 8–9 = 0,
  byte 10 = `'C'` (client originated), byte 11 = `'M'` (microphone). This follows
  Sunshine's existing convention (`CR`/`HR` RTSP, `CC`/`HC` control, `V` video),
  so a mic nonce can never equal any other stream's nonce under the same key.
- Integrity: there is no AAD (neither GCM helper supports it), so everything
  meaningful is either encrypted or bound:
  - `flags` and `rtpTs` are inside the ciphertext;
  - `seq` is part of the nonce, so changing it breaks the tag;
  - `sessionTag` only selects the session, whose own key must then verify the tag;
  - an unknown `type`/`version` is dropped.
- Replay protection: the host keeps the highest accepted `seq` and a 64-entry
  bitmap. It drops duplicates and packets older than the window.
- Mic packets are always encrypted. There is no plaintext mode.

### 1.4 Audio

- Opus, 48 kHz, **mono**, 20 ms frames (960 samples), VBR about 32 kbit/s, in-band
  FEC on, `OPUS_APPLICATION_VOIP`.
- The client stops sending while muted. The host plays silence when no packet
  arrived for 100 ms, and destroys nothing.

### 1.5 Host sink

While Sunshine runs, it keeps a PulseAudio/PipeWire null sink
`sink-clouddeck-mic` plus a remapped source **`clouddeck-mic`**
("CloudDeck Microphone"). Decoded audio is played into the sink. Games,
Discord and Steam pick up the source as an ordinary microphone.

### 1.6 Failure behaviour

| Situation | Result |
|---|---|
| Host doesn't advertise v1 | Client hides the mic UI ("host does not support microphone") |
| GCM tag invalid / replay | Packet dropped, counted in the host log at debug level |
| Client sends mic packets without negotiation | Host drops them (no session has mic enabled) |
| Stock Sunshine receives a mic packet | Ignored: no ping payload starts with byte 0x8D |
