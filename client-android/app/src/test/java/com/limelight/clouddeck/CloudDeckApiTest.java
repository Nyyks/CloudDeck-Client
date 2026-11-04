package com.limelight.clouddeck;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Same cases as client-desktop/tests/clouddeck/tst_clouddeckapi.cpp. */
public class CloudDeckApiTest {
    private static JSONObject fixture(String name) throws Exception {
        String dir = System.getProperty("fixtures.dir", "../../shared/fixtures/clouddeck");
        byte[] b = Files.readAllBytes(new File(dir, name).toPath());
        return new JSONObject(new String(b, StandardCharsets.UTF_8));
    }

    private static String b64url(byte[] b) {
        String s = java.util.Base64.getUrlEncoder().withoutPadding().encodeToString(b);
        return s;
    }

    @Test
    public void allowlistMatchesSharedFixture() throws Exception {
        JSONArray cases = fixture("allowlist.json").getJSONArray("cases");
        assertTrue(cases.length() > 10);
        for (int i = 0; i < cases.length(); i++) {
            JSONObject c = cases.getJSONObject(i);
            String method = c.getString("method");
            String path = c.getString("path");
            assertEquals(method + " " + path, c.getBoolean("allowed"), CloudDeckApi.isRequestAllowed(method, path));
        }
    }

    @Test
    public void parsesRunningMachine() throws Exception {
        CloudDeckApi.Machine m = CloudDeckApi.parseMachine(fixture("machine_running.json"));
        assertTrue(m.valid);
        assertEquals("m_43ca6bbd2b", m.id);
        assertEquals("running", m.status);
        assertEquals("203.0.113.10", m.publicIp);
        assertEquals("eu-de-nbg-1", m.region);
        assertEquals(1793799500000L, m.lastStartedMs);
    }

    @Test
    public void parsesOffMachine() throws Exception {
        CloudDeckApi.Machine m = CloudDeckApi.parseMachine(fixture("machine_off.json"));
        assertTrue(m.valid);
        assertEquals("off", m.status);
        assertEquals("", m.publicIp);
    }

    @Test
    public void rejectsInvalidMachine() throws Exception {
        assertFalse(CloudDeckApi.parseMachine(new JSONObject()).valid);
        assertFalse(CloudDeckApi.parseMachine(new JSONObject("{\"id\":\"x_1\",\"status\":\"off\"}")).valid);
        assertFalse(CloudDeckApi.parseMachine(new JSONObject("{\"id\":\"m_1\"}")).valid);
    }

    @Test
    public void hostAddress() {
        assertEquals("43ca6bbd2b.my.clouddeck.app", CloudDeckApi.hostAddressFor("m_43ca6bbd2b"));
        assertEquals("", CloudDeckApi.hostAddressFor(""));
        assertEquals("", CloudDeckApi.hostAddressFor("m_"));
        assertEquals("", CloudDeckApi.hostAddressFor("43ca6bbd2b"));
        assertEquals("", CloudDeckApi.hostAddressFor(null));
    }

    @Test
    public void jwtClaims() throws Exception {
        JSONObject claims = new JSONObject();
        claims.put("custom:account", "a_7d7306fe31");
        claims.put("email", "user@example.com");
        claims.put("x", "??>>??>>");
        String jwt = b64url("{\"alg\":\"RS256\"}".getBytes(StandardCharsets.UTF_8)) + "." +
                b64url(claims.toString().getBytes(StandardCharsets.UTF_8)) + ".sig";
        JSONObject d = CloudDeckApi.decodeJwtClaims(jwt);
        assertEquals("a_7d7306fe31", d.getString("custom:account"));
        assertEquals("user@example.com", d.getString("email"));
        assertEquals("??>>??>>", d.getString("x"));
    }

    @Test
    public void jwtMalformed() {
        assertEquals(0, CloudDeckApi.decodeJwtClaims("").length());
        assertEquals(0, CloudDeckApi.decodeJwtClaims("abc").length());
        assertEquals(0, CloudDeckApi.decodeJwtClaims("a..c").length());
        assertEquals(0, CloudDeckApi.decodeJwtClaims("a.!!!notbase64!!!.c").length());
        assertEquals(0, CloudDeckApi.decodeJwtClaims("a.b.c.d").length());
        assertEquals(0, CloudDeckApi.decodeJwtClaims(null).length());
    }

    @Test
    public void base64UrlRoundTrip() {
        for (int n = 0; n < 40; n++) {
            byte[] data = new byte[n];
            for (int i = 0; i < n; i++) data[i] = (byte) (i * 37 + n);
            byte[] back = CloudDeckApi.base64UrlDecode(b64url(data));
            assertTrue(java.util.Arrays.equals(data, back));
        }
    }

    @Test
    public void transitional() {
        assertTrue(CloudDeckApi.isTransitional("starting"));
        assertTrue(CloudDeckApi.isTransitional("stopping"));
        assertFalse(CloudDeckApi.isTransitional("running"));
        assertFalse(CloudDeckApi.isTransitional("off"));
    }
}
