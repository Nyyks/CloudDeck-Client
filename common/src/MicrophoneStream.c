// CloudDeck-Client: microphone stream (client -> host), protocol v1.
// Wire format and negotiation: docs/protocol-extensions.md section 1.
#include "Limelight-internal.h"

#define MIC_PACKET_TYPE     0x8D
#define MIC_PACKET_VERSION  1
#define MIC_HEADER_SIZE     28  // type, version, reserved[2], sessionTag[16], seq[8]
#define MIC_TAG_SIZE        16
#define MIC_INNER_SIZE      6   // flags (u16) + rtpTs (u32), encrypted
#define MIC_MAX_OPUS_SIZE   1200

bool MicSupportedByHost;
bool MicNegotiated;
char MicSessionTag[16];

static PLT_MUTEX micLock;
static bool micLockInitialized;
static bool micActive;
static PPLT_CRYPTO_CONTEXT micCryptoContext;
static uint64_t micSequenceNumber;
static uint32_t micRtpTimestamp;

static void putBe16(unsigned char* p, uint16_t v) { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
static void putBe32(unsigned char* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (unsigned char)(v >> (24 - 8 * i)); }
static void putBe64(unsigned char* p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (unsigned char)(v >> (56 - 8 * i)); }

void buildMicrophoneNonce(uint64_t seq, unsigned char nonce[12]) {
    for (int i = 0; i < 8; i++) {
        nonce[i] = (unsigned char)(seq >> (8 * i)); // little-endian, like Sunshine's other GCM streams
    }
    nonce[8] = 0;
    nonce[9] = 0;
    nonce[10] = 'C'; // client originated
    nonce[11] = 'M'; // microphone
}

int buildMicrophonePacket(PPLT_CRYPTO_CONTEXT cryptoContext, const unsigned char key[16], const char sessionTag[16],
                          uint64_t seq, uint32_t rtpTimestamp, uint16_t flags,
                          const unsigned char* opusData, int opusLength,
                          unsigned char* out, int outCapacity) {
    unsigned char plaintext[MIC_INNER_SIZE + MIC_MAX_OPUS_SIZE];
    unsigned char nonce[12];
    int cipherLength;

    if (opusData == NULL || opusLength <= 0 || opusLength > MIC_MAX_OPUS_SIZE) {
        return -1;
    }
    if (outCapacity < MIC_HEADER_SIZE + MIC_TAG_SIZE + MIC_INNER_SIZE + opusLength) {
        return -1;
    }

    out[0] = MIC_PACKET_TYPE;
    out[1] = MIC_PACKET_VERSION;
    out[2] = 0;
    out[3] = 0;
    memcpy(&out[4], sessionTag, 16);
    putBe64(&out[20], seq);

    putBe16(&plaintext[0], flags);
    putBe32(&plaintext[2], rtpTimestamp);
    memcpy(&plaintext[MIC_INNER_SIZE], opusData, opusLength);

    buildMicrophoneNonce(seq, nonce);
    cipherLength = MIC_INNER_SIZE + opusLength;
    if (!PltEncryptMessage(cryptoContext, ALGORITHM_AES_GCM, 0,
                           (unsigned char*)key, 16,
                           nonce, sizeof(nonce),
                           &out[MIC_HEADER_SIZE], MIC_TAG_SIZE,             // tag right after the header
                           plaintext, MIC_INNER_SIZE + opusLength,
                           &out[MIC_HEADER_SIZE + MIC_TAG_SIZE], &cipherLength)) {
        return -1;
    }

    return MIC_HEADER_SIZE + MIC_TAG_SIZE + cipherLength;
}

int parseMicrophonePacket(PPLT_CRYPTO_CONTEXT cryptoContext, const unsigned char key[16],
                          const unsigned char* packet, int packetLength,
                          char sessionTagOut[16], uint64_t* seqOut, uint32_t* rtpTimestampOut, uint16_t* flagsOut,
                          unsigned char* opusOut, int opusCapacity) {
    unsigned char plaintext[MIC_INNER_SIZE + MIC_MAX_OPUS_SIZE];
    unsigned char nonce[12];
    unsigned char tag[MIC_TAG_SIZE];
    int plainLength;
    int cipherLength = packetLength - MIC_HEADER_SIZE - MIC_TAG_SIZE;
    uint64_t seq = 0;

    if (packetLength < MIC_HEADER_SIZE + MIC_TAG_SIZE + MIC_INNER_SIZE + 1 ||
        cipherLength > (int)sizeof(plaintext) ||
        packet[0] != MIC_PACKET_TYPE || packet[1] != MIC_PACKET_VERSION) {
        return -1;
    }
    for (int i = 0; i < 8; i++) {
        seq = (seq << 8) | packet[20 + i];
    }

    buildMicrophoneNonce(seq, nonce);
    memcpy(tag, &packet[MIC_HEADER_SIZE], MIC_TAG_SIZE);
    plainLength = cipherLength;
    if (!PltDecryptMessage(cryptoContext, ALGORITHM_AES_GCM, 0,
                           (unsigned char*)key, 16,
                           nonce, sizeof(nonce),
                           tag, MIC_TAG_SIZE,
                           (unsigned char*)&packet[MIC_HEADER_SIZE + MIC_TAG_SIZE], cipherLength,
                           plaintext, &plainLength)) {
        return -1;
    }
    if (plainLength - MIC_INNER_SIZE > opusCapacity) {
        return -1;
    }

    memcpy(sessionTagOut, &packet[4], 16);
    *seqOut = seq;
    *flagsOut = (uint16_t)((plaintext[0] << 8) | plaintext[1]);
    *rtpTimestampOut = ((uint32_t)plaintext[2] << 24) | ((uint32_t)plaintext[3] << 16) |
                       ((uint32_t)plaintext[4] << 8) | plaintext[5];
    memcpy(opusOut, &plaintext[MIC_INNER_SIZE], plainLength - MIC_INNER_SIZE);
    return plainLength - MIC_INNER_SIZE;
}

void startMicrophoneStream(void) {
    if (!micLockInitialized) {
        // Lives for the process: senders may call in around connection start and stop.
        PltCreatePersistentMutex(&micLock);
        micLockInitialized = true;
    }
    PltLockMutex(&micLock);
    if (MicNegotiated && !micActive) {
        micCryptoContext = PltCreateCryptoContext();
        micSequenceNumber = 0;
        micRtpTimestamp = 0;
        micActive = micCryptoContext != NULL;
        Limelog("Microphone stream %s\n", micActive ? "active" : "failed to start");
    }
    PltUnlockMutex(&micLock);
}

void stopMicrophoneStream(void) {
    if (!micLockInitialized) {
        return;
    }
    PltLockMutex(&micLock);
    if (micActive) {
        PltDestroyCryptoContext(micCryptoContext);
        micCryptoContext = NULL;
        micActive = false;
    }
    MicNegotiated = false;
    PltUnlockMutex(&micLock);
}

bool LiIsMicrophoneSupportedByHost(void) {
    return MicSupportedByHost;
}

bool LiIsMicrophoneStreamActive(void) {
    return micActive;
}

int LiSendMicrophoneOpusData(const unsigned char* opusData, int opusLength, unsigned int frameSamples) {
    unsigned char packet[MIC_HEADER_SIZE + MIC_TAG_SIZE + MIC_INNER_SIZE + MIC_MAX_OPUS_SIZE];
    int len;
    int err = 0;

    if (!micLockInitialized) {
        return -1;
    }
    PltLockMutex(&micLock);
    if (!micActive) {
        err = -1;
    }
    else {
        len = buildMicrophonePacket(micCryptoContext, (const unsigned char*)StreamConfig.remoteInputAesKey,
                                    MicSessionTag, micSequenceNumber, micRtpTimestamp, 0,
                                    opusData, opusLength, packet, sizeof(packet));
        if (len < 0) {
            err = -1;
        }
        else {
            micSequenceNumber++;
            micRtpTimestamp += frameSamples;
            err = sendAudioSocketDatagram(packet, len) == len ? 0 : -1;
        }
    }
    PltUnlockMutex(&micLock);
    return err;
}
