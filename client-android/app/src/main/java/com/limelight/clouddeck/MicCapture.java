package com.limelight.clouddeck;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;

import com.limelight.LimeLog;
import com.limelight.nvstream.jni.MoonBridge;

/**
 * CloudDeck-Client: captures the microphone (VOICE_COMMUNICATION with platform echo
 * cancellation) and sends 20 ms Opus frames through the encrypted mic stream
 * (docs/protocol-extensions.md §1).
 */
public final class MicCapture {
    public static final String PREF_ENABLED = "micEnabled";
    private static final int RATE = 48000;
    private static final int FRAME = 960;

    private static MicCapture active;

    private final AudioRecord record;
    private final long encoder;
    private AcousticEchoCanceler aec;
    private NoiseSuppressor ns;
    private Thread thread;
    private volatile boolean running;
    private volatile boolean muted;

    public static boolean isEnabled(Context ctx) {
        return ctx.getSharedPreferences("clouddeck", Context.MODE_PRIVATE).getBoolean(PREF_ENABLED, false)
                && hasPermission(ctx);
    }

    public static boolean hasPermission(Context ctx) {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.M ||
                ctx.checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED;
    }

    /** Call before the connection starts. */
    public static void prepare(Context ctx) {
        MoonBridge.setMicrophoneRequested(isEnabled(ctx));
    }

    /** Call once the connection is up. No-op unless the mic was negotiated. */
    public static synchronized void startIfNegotiated(Context ctx) {
        if (active != null || !isEnabled(ctx)) return;
        if (!MoonBridge.isMicrophoneStreamActive()) {
            LimeLog.info("Mic: not negotiated (host supports mic: " + MoonBridge.isMicrophoneSupportedByHost() + ")");
            return;
        }
        try {
            active = new MicCapture();
            active.start();
        } catch (Exception e) {
            LimeLog.warning("Mic: could not start capture: " + e);
            active = null;
        }
    }

    public static synchronized void stopActive() {
        if (active != null) {
            active.stop();
            active = null;
        }
    }

    public static synchronized boolean toggleMute() {
        if (active == null) return false;
        active.muted = !active.muted;
        return active.muted;
    }

    @SuppressLint("MissingPermission") // checked by isEnabled()
    private MicCapture() {
        int min = AudioRecord.getMinBufferSize(RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        record = new AudioRecord(MediaRecorder.AudioSource.VOICE_COMMUNICATION, RATE,
                AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, Math.max(min, FRAME * 2 * 4));
        if (record.getState() != AudioRecord.STATE_INITIALIZED) {
            record.release();
            throw new IllegalStateException("AudioRecord not initialized");
        }
        if (AcousticEchoCanceler.isAvailable()) {
            aec = AcousticEchoCanceler.create(record.getAudioSessionId());
            if (aec != null) aec.setEnabled(true);
        }
        if (NoiseSuppressor.isAvailable()) {
            ns = NoiseSuppressor.create(record.getAudioSessionId());
            if (ns != null) ns.setEnabled(true);
        }
        encoder = MoonBridge.micEncoderCreate();
        if (encoder == 0) {
            record.release();
            throw new IllegalStateException("Opus encoder creation failed");
        }
    }

    private void start() {
        running = true;
        record.startRecording();
        thread = new Thread(() -> {
            short[] frame = new short[FRAME];
            while (running) {
                int off = 0;
                while (off < FRAME && running) {
                    int n = record.read(frame, off, FRAME - off);
                    if (n <= 0) break;
                    off += n;
                }
                if (!running || off < FRAME) continue;
                if (!muted) MoonBridge.micEncodeAndSend(encoder, frame);
            }
        }, "CloudDeck-Mic");
        thread.start();
        LimeLog.info("Mic: streaming microphone to host (AEC " + (aec != null) + ")");
    }

    private void stop() {
        running = false;
        try { record.stop(); } catch (IllegalStateException ignored) {}
        if (thread != null) {
            // record.stop() unblocks read(); wait fully so the encoder isn't freed under the thread.
            try { thread.join(); } catch (InterruptedException ignored) {}
        }
        if (aec != null) aec.release();
        if (ns != null) ns.release();
        record.release();
        MoonBridge.micEncoderDestroy(encoder);
    }
}
