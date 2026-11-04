# Microphone protocol test vector

`vector1.hex` is one mic v1 packet (docs/protocol-extensions.md §1) built with:

| Field | Value |
|---|---|
| key (rikey) | `00112233445566778899aabbccddeeff` |
| sessionTag | ASCII `0123456789ABCDEF` |
| seq | 42 |
| rtpTs | 40320 |
| flags | 0 |
| Opus payload (20 bytes) | byte i = (i*7 + 3) mod 256 |

The client encoder (`tests/mic-protocol`) must produce it byte for byte, and the
host decoder (Sunshine patch) must decode it back to those values.
