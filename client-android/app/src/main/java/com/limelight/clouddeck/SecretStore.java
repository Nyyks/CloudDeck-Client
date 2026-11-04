package com.limelight.clouddeck;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import com.limelight.LimeLog;

import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.HashMap;
import java.util.Map;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/**
 * Stores small secrets (the CloudDeck refresh token) encrypted with an AES-256-GCM
 * key that never leaves the Android Keystore. Only the ciphertext and IV are kept in
 * SharedPreferences. On API &lt; 23 (no Keystore AES) secrets live in memory only, so
 * the user signs in again on each launch. There is no plaintext fallback.
 */
public final class SecretStore {
    private static final String KEYSTORE = "AndroidKeyStore";
    private static final String KEY_ALIAS = "clouddeck_client_secrets_v1";
    private static final String PREFS = "clouddeck_secrets";
    private static final int GCM_TAG_BITS = 128;

    private static final Map<String, String> memoryOnly = new HashMap<>();

    private final SharedPreferences prefs;

    public SecretStore(Context context) {
        prefs = context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    public static String backendName() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? "Android Keystore" : "memory only";
    }

    /** @return false if the secret could only be kept in memory. */
    public synchronized boolean write(String key, String secret) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            try {
                Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
                c.init(Cipher.ENCRYPT_MODE, getOrCreateKey());
                byte[] ct = c.doFinal(secret.getBytes(StandardCharsets.UTF_8));
                prefs.edit()
                        .putString(key + ".iv", Base64.encodeToString(c.getIV(), Base64.NO_WRAP))
                        .putString(key + ".ct", Base64.encodeToString(ct, Base64.NO_WRAP))
                        .apply();
                memoryOnly.remove(key);
                return true;
            } catch (Exception e) {
                LimeLog.warning("SecretStore: keystore write failed: " + e);
            }
        }
        memoryOnly.put(key, secret);
        return false;
    }

    public synchronized String read(String key) {
        if (memoryOnly.containsKey(key)) {
            return memoryOnly.get(key);
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return null;
        }
        String iv = prefs.getString(key + ".iv", null);
        String ct = prefs.getString(key + ".ct", null);
        if (iv == null || ct == null) {
            return null;
        }
        try {
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.DECRYPT_MODE, getOrCreateKey(),
                    new GCMParameterSpec(GCM_TAG_BITS, Base64.decode(iv, Base64.NO_WRAP)));
            return new String(c.doFinal(Base64.decode(ct, Base64.NO_WRAP)), StandardCharsets.UTF_8);
        } catch (Exception e) {
            // Key invalidated (e.g. device credentials changed) or data corrupt: forget it.
            LimeLog.warning("SecretStore: keystore read failed, clearing: " + e);
            remove(key);
            return null;
        }
    }

    public synchronized void remove(String key) {
        memoryOnly.remove(key);
        prefs.edit().remove(key + ".iv").remove(key + ".ct").apply();
    }

    private static SecretKey getOrCreateKey() throws Exception {
        KeyStore ks = KeyStore.getInstance(KEYSTORE);
        ks.load(null);
        KeyStore.Entry entry = ks.getEntry(KEY_ALIAS, null);
        if (entry instanceof KeyStore.SecretKeyEntry) {
            return ((KeyStore.SecretKeyEntry) entry).getSecretKey();
        }
        KeyGenerator kg = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE);
        kg.init(new KeyGenParameterSpec.Builder(KEY_ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build());
        return kg.generateKey();
    }
}
