// Unit tests for the microphone packet format (docs/protocol-extensions.md §1),
// linked against the real moonlight-common-c build from common/.
// Also emits/validates shared/fixtures/mic/vector1.hex, the cross-implementation
// vector the Sunshine patch's decoder test must accept.
#include "Limelight-internal.h"

#include <stdio.h>
#include <stdlib.h>

static int failures;
#define CHECK(cond, msg) do { if (cond) { printf("ok   - %s\n", msg); } else { printf("FAIL - %s\n", msg); failures++; } } while (0)

static const unsigned char kKey[16] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
static const char kTag[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };

static void toHex(const unsigned char* b, int n, char* out) {
    static const char* h = "0123456789abcdef";
    for (int i = 0; i < n; i++) { out[2 * i] = h[b[i] >> 4]; out[2 * i + 1] = h[b[i] & 15]; }
    out[2 * n] = 0;
}

int main(int argc, char** argv) {
    PPLT_CRYPTO_CONTEXT ctx = PltCreateCryptoContext();
    unsigned char opus[100], pkt[1400], out[1400];
    char tagOut[16];
    uint64_t seq; uint32_t ts; uint16_t flags;
    int n, m;

    for (int i = 0; i < (int)sizeof(opus); i++) opus[i] = (unsigned char)(i * 7 + 3);

    // Nonce layout
    unsigned char nonce[12];
    buildMicrophoneNonce(0x0102030405060708ULL, nonce);
    CHECK(nonce[0] == 0x08 && nonce[7] == 0x01 && nonce[8] == 0 && nonce[9] == 0 && nonce[10] == 'C' && nonce[11] == 'M',
          "nonce = LE64(seq) || 00 00 || 'C' 'M'");

    // Header layout
    n = buildMicrophonePacket(ctx, kKey, kTag, 0x0102030405060708ULL, 960, 1, opus, sizeof(opus), pkt, sizeof(pkt));
    CHECK(n == 28 + 16 + 6 + (int)sizeof(opus), "packet length = 28 header + 16 tag + 6 inner + opus");
    CHECK(pkt[0] == 0x8D && pkt[1] == 1 && pkt[2] == 0 && pkt[3] == 0, "type 0x8D, version 1, reserved 0");
    CHECK(memcmp(&pkt[4], kTag, 16) == 0, "session tag at offset 4");
    CHECK(pkt[20] == 0x01 && pkt[27] == 0x08, "seq big-endian at offset 20");
    CHECK(memmem(pkt, n, opus, sizeof(opus)) == NULL, "opus payload not visible in clear text");

    // Round trip
    m = parseMicrophonePacket(ctx, kKey, pkt, n, tagOut, &seq, &ts, &flags, out, sizeof(out));
    CHECK(m == (int)sizeof(opus) && memcmp(out, opus, sizeof(opus)) == 0, "round trip restores opus payload");
    CHECK(seq == 0x0102030405060708ULL && ts == 960 && flags == 1 && memcmp(tagOut, kTag, 16) == 0,
          "round trip restores seq, rtpTs, flags, session tag");

    // Tampering
    unsigned char bad[1400];
    unsigned char wrongKey[16]; memcpy(wrongKey, kKey, 16); wrongKey[0] ^= 1;
    {
        // A PLT crypto context binds to the first key it sees (one context per
        // session in production), so the wrong key needs its own context.
        PPLT_CRYPTO_CONTEXT other = PltCreateCryptoContext();
        CHECK(parseMicrophonePacket(other, wrongKey, pkt, n, tagOut, &seq, &ts, &flags, out, sizeof(out)) < 0, "wrong key rejected");
        PltDestroyCryptoContext(other);
    }
    memcpy(bad, pkt, n); bad[27] ^= 1;
    CHECK(parseMicrophonePacket(ctx, kKey, bad, n, tagOut, &seq, &ts, &flags, out, sizeof(out)) < 0, "modified seq rejected (nonce-bound)");
    memcpy(bad, pkt, n); bad[30] ^= 1;
    CHECK(parseMicrophonePacket(ctx, kKey, bad, n, tagOut, &seq, &ts, &flags, out, sizeof(out)) < 0, "modified GCM tag rejected");
    memcpy(bad, pkt, n); bad[n - 1] ^= 1;
    CHECK(parseMicrophonePacket(ctx, kKey, bad, n, tagOut, &seq, &ts, &flags, out, sizeof(out)) < 0, "modified ciphertext rejected");
    memcpy(bad, pkt, n); bad[46] ^= 1;
    CHECK(parseMicrophonePacket(ctx, kKey, bad, n, tagOut, &seq, &ts, &flags, out, sizeof(out)) < 0, "modified encrypted rtpTs/flags rejected");
    memcpy(bad, pkt, n); bad[1] = 2;
    CHECK(parseMicrophonePacket(ctx, kKey, bad, n, tagOut, &seq, &ts, &flags, out, sizeof(out)) < 0, "unknown version rejected");
    CHECK(parseMicrophonePacket(ctx, kKey, pkt, 50, tagOut, &seq, &ts, &flags, out, sizeof(out)) < 0, "truncated packet rejected");

    // Limits
    unsigned char big[1201] = { 0 };
    CHECK(buildMicrophonePacket(ctx, kKey, kTag, 1, 0, 0, big, 1201, pkt, sizeof(pkt)) < 0, "opus > 1200 bytes refused");
    CHECK(buildMicrophonePacket(ctx, kKey, kTag, 1, 0, 0, opus, 0, pkt, sizeof(pkt)) < 0, "empty opus refused");
    CHECK(buildMicrophonePacket(ctx, kKey, kTag, 1, 0, 0, opus, sizeof(opus), pkt, 60) < 0, "small output buffer refused");

    // Distinct seq -> distinct ciphertext (nonce never reused)
    unsigned char p1[1400], p2[1400];
    int l1 = buildMicrophonePacket(ctx, kKey, kTag, 7, 0, 0, opus, sizeof(opus), p1, sizeof(p1));
    int l2 = buildMicrophonePacket(ctx, kKey, kTag, 8, 0, 0, opus, sizeof(opus), p2, sizeof(p2));
    CHECK(l1 == l2 && memcmp(p1 + 28, p2 + 28, l1 - 28) != 0, "different seq gives different tag+ciphertext");

    // Cross-implementation vector: key=kKey, tag=kTag, seq=42, rtpTs=40320, flags=0, opus=first 20 bytes
    int lv = buildMicrophonePacket(ctx, kKey, kTag, 42, 40320, 0, opus, 20, p1, sizeof(p1));
    char hex[2 * 1400 + 1];
    toHex(p1, lv, hex);
    if (argc > 1) {
        FILE* f = fopen(argv[1], "rb");
        char want[2 * 1400 + 2] = { 0 };
        if (f) { if (!fgets(want, sizeof(want), f)) want[0] = 0; fclose(f); }
        want[strcspn(want, "\r\n")] = 0;
        CHECK(strcmp(want, hex) == 0, "packet matches shared/fixtures/mic/vector1.hex");
    }
    else {
        printf("vector1: %s\n", hex);
    }

    PltDestroyCryptoContext(ctx);
    printf(failures ? "%d FAILED\n" : "all mic protocol tests passed\n", failures);
    return failures ? 1 : 0;
}
