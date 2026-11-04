package com.limelight.clouddeck;

import org.json.JSONException;
import org.json.JSONObject;

import java.nio.charset.StandardCharsets;
import java.util.regex.Pattern;

/**
 * Pure (network-free) helpers for the CloudDeck portal API. Mirrors
 * client-desktop/app/clouddeck/clouddeckapi.cpp; both are tested against
 * shared/fixtures/clouddeck. Spec: docs/clouddeck-api.md.
 */
public final class CloudDeckApi {
    public static final String API_BASE = "https://api.clouddeck.app";
    public static final String COGNITO_ENDPOINT = "https://cognito-idp.eu-central-1.amazonaws.com/";
    public static final String COGNITO_CLIENT_ID = "2e7an7pt3vqdae0abgskfs38k8";
    public static final String HOST_SUFFIX = ".my.clouddeck.app";

    public static final String STATUS_OFF = "off";
    public static final String STATUS_STARTING = "starting";
    public static final String STATUS_RUNNING = "running";
    public static final String STATUS_STOPPING = "stopping";

    private static final Pattern GET_ACCOUNT = Pattern.compile("^/accounts/a_[A-Za-z0-9]+$");
    private static final Pattern GET_MACHINE = Pattern.compile("^/machines/m_[A-Za-z0-9]+$");
    private static final Pattern POST_MACHINE = Pattern.compile("^/machines/m_[A-Za-z0-9]+/(start|stop|clients)$");

    private CloudDeckApi() {}

    public static final class Machine {
        public String id = "";
        public String status = "";
        public String publicIp = "";
        public String os = "";
        public String region = "";
        public long lastStartedMs;
        public boolean valid;
    }

    /**
     * Endpoint allowlist. Everything else, notably POST /machines/{id}/reset
     * (factory wipe), billing and account mutation, is refused before sending.
     */
    public static boolean isRequestAllowed(String method, String path) {
        if (method == null || path == null) {
            return false;
        }
        if (method.equals("GET")) {
            return GET_ACCOUNT.matcher(path).matches() || GET_MACHINE.matcher(path).matches();
        }
        if (method.equals("POST")) {
            return POST_MACHINE.matcher(path).matches();
        }
        return false;
    }

    /** Parses GET /machines/{id}. Never reads the "password" field. */
    public static Machine parseMachine(JSONObject o) {
        Machine m = new Machine();
        m.id = o.optString("id", "");
        m.status = o.optString("status", "");
        m.publicIp = o.optString("public_ip", "");
        m.os = o.optString("os", "");
        m.region = o.optString("region", "");
        m.lastStartedMs = o.optLong("last_started", 0);
        m.valid = m.id.startsWith("m_") && m.id.length() > 2 && !m.status.isEmpty();
        return m;
    }

    /** "m_43ca6bbd2b" -> "43ca6bbd2b.my.clouddeck.app" (same rule as the portal). */
    public static String hostAddressFor(String machineId) {
        if (machineId == null || !machineId.startsWith("m_") || machineId.length() <= 2) {
            return "";
        }
        return machineId.substring(2) + HOST_SUFFIX;
    }

    /** Decodes a JWT payload without verification; empty object on malformed input. */
    public static JSONObject decodeJwtClaims(String jwt) {
        if (jwt == null) {
            return new JSONObject();
        }
        String[] parts = jwt.split("\\.", -1);
        if (parts.length != 3 || parts[1].isEmpty()) {
            return new JSONObject();
        }
        byte[] payload = base64UrlDecode(parts[1]);
        if (payload == null) {
            return new JSONObject();
        }
        try {
            return new JSONObject(new String(payload, StandardCharsets.UTF_8));
        } catch (JSONException e) {
            return new JSONObject();
        }
    }

    public static boolean isTransitional(String status) {
        return STATUS_STARTING.equals(status) || STATUS_STOPPING.equals(status) ||
                "pending".equals(status) || "provisioning".equals(status);
    }

    /** RFC 4648 section 5 decoder (minSdk 21 has no java.util.Base64). Null on invalid input. */
    static byte[] base64UrlDecode(String s) {
        int len = s.length();
        while (len > 0 && s.charAt(len - 1) == '=') {
            len--;
        }
        if (len % 4 == 1) {
            return null;
        }
        byte[] out = new byte[len * 3 / 4];
        int buf = 0, bits = 0, o = 0;
        for (int i = 0; i < len; i++) {
            char c = s.charAt(i);
            int v;
            if (c >= 'A' && c <= 'Z') v = c - 'A';
            else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
            else if (c >= '0' && c <= '9') v = c - '0' + 52;
            else if (c == '-' || c == '+') v = 62;
            else if (c == '_' || c == '/') v = 63;
            else return null;
            buf = (buf << 6) | v;
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out[o++] = (byte) ((buf >> bits) & 0xFF);
            }
        }
        return o == out.length ? out : java.util.Arrays.copyOf(out, o);
    }
}
