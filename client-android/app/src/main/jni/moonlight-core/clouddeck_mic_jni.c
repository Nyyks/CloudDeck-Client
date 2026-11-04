// CloudDeck-Client: JNI glue for the microphone stream (docs/protocol-extensions.md §1).
#include <Limelight.h>
#include <jni.h>
#include <opus.h>
#include <stdbool.h>
#include <stdlib.h>

#define MIC_FRAME_SAMPLES 960   // 20 ms at 48 kHz
#define MIC_MAX_PACKET    1200

// Read by MoonBridge.startConnection (callbacks.c) into STREAM_CONFIGURATION.enableMicrophone
bool CdcMicRequested;

JNIEXPORT void JNICALL
Java_com_limelight_nvstream_jni_MoonBridge_setMicrophoneRequested(JNIEnv *env, jclass clazz, jboolean requested) {
    CdcMicRequested = requested;
}

JNIEXPORT jboolean JNICALL
Java_com_limelight_nvstream_jni_MoonBridge_isMicrophoneSupportedByHost(JNIEnv *env, jclass clazz) {
    return LiIsMicrophoneSupportedByHost();
}

JNIEXPORT jboolean JNICALL
Java_com_limelight_nvstream_jni_MoonBridge_isMicrophoneStreamActive(JNIEnv *env, jclass clazz) {
    return LiIsMicrophoneStreamActive();
}

JNIEXPORT jlong JNICALL
Java_com_limelight_nvstream_jni_MoonBridge_micEncoderCreate(JNIEnv *env, jclass clazz) {
    int err = 0;
    OpusEncoder* enc = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
    if (enc == NULL) {
        return 0;
    }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(32000));
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(10));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    return (jlong)(intptr_t)enc;
}

// Encodes one 960-sample mono frame and sends it. Returns 0 on success, <0 on error.
JNIEXPORT jint JNICALL
Java_com_limelight_nvstream_jni_MoonBridge_micEncodeAndSend(JNIEnv *env, jclass clazz, jlong handle, jshortArray pcm) {
    OpusEncoder* enc = (OpusEncoder*)(intptr_t)handle;
    unsigned char packet[MIC_MAX_PACKET];
    jint ret;

    if (enc == NULL || (*env)->GetArrayLength(env, pcm) < MIC_FRAME_SAMPLES) {
        return -1;
    }
    jshort* samples = (*env)->GetShortArrayElements(env, pcm, NULL);
    int n = opus_encode(enc, samples, MIC_FRAME_SAMPLES, packet, sizeof(packet));
    (*env)->ReleaseShortArrayElements(env, pcm, samples, JNI_ABORT);
    if (n <= 0) {
        return -2;
    }
    ret = LiSendMicrophoneOpusData(packet, n, MIC_FRAME_SAMPLES);
    return ret;
}

JNIEXPORT void JNICALL
Java_com_limelight_nvstream_jni_MoonBridge_micEncoderDestroy(JNIEnv *env, jclass clazz, jlong handle) {
    if (handle != 0) {
        opus_encoder_destroy((OpusEncoder*)(intptr_t)handle);
    }
}
