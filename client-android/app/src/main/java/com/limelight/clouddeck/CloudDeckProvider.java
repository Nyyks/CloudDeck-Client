package com.limelight.clouddeck;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;
import okhttp3.ResponseBody;

/**
 * CloudDeck implementation of the CloudProvider contract (see the desktop
 * cloudprovider.h and docs/clouddeck-api.md). All methods are blocking and must be
 * called off the main thread. Only the refresh token is persisted (SecretStore).
 */
public class CloudDeckProvider {
    public static class CloudException extends Exception {
        public final int httpStatus;
        public CloudException(String msg, int httpStatus) { super(msg); this.httpStatus = httpStatus; }
    }

    public enum SignInResult { OK, MFA_REQUIRED }

    private static final String REFRESH_KEY = "clouddeck.refresh_token";
    private static final String PREFS = "clouddeck";
    private static final MediaType JSON = MediaType.parse("application/json");
    private static final MediaType AMZ_JSON = MediaType.parse("application/x-amz-json-1.1");

    private final OkHttpClient http = new OkHttpClient.Builder()
            .connectTimeout(15, TimeUnit.SECONDS)
            .readTimeout(20, TimeUnit.SECONDS)
            .followRedirects(false)
            .build();
    private final SecretStore secrets;
    private final SharedPreferences prefs;

    private String accessToken, idToken, refreshToken;
    private long accessExpiryMs;
    private String accountId, machineId, email;
    private String challengeName, challengeSession, challengeUser;

    public CloudDeckProvider(Context context) {
        secrets = new SecretStore(context);
        prefs = context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        email = prefs.getString("email", "");
    }

    public synchronized boolean isSignedIn() { return refreshToken != null && !refreshToken.isEmpty(); }
    public synchronized String getEmail() { return email; }

    // ------------------------------------------------------------------ transport

    private static String errorMessage(JSONObject o, Response r) {
        String m = o.optString("message", "");
        return m.isEmpty() ? ("HTTP " + r.code()) : m;
    }

    private static JSONObject readJson(Response r) throws IOException {
        ResponseBody body = r.body();
        String s = body != null ? body.string() : "";
        try {
            return s.isEmpty() ? new JSONObject() : new JSONObject(s);
        } catch (JSONException e) {
            return new JSONObject();
        }
    }

    private JSONObject cognito(String target, JSONObject body) throws IOException, CloudException {
        Request req = new Request.Builder()
                .url(CloudDeckApi.COGNITO_ENDPOINT)
                .header("X-Amz-Target", "AWSCognitoIdentityProviderService." + target)
                .post(RequestBody.create(body.toString(), AMZ_JSON))
                .build();
        try (Response r = http.newCall(req).execute()) {
            JSONObject o = readJson(r);
            if (!r.isSuccessful()) {
                throw new CloudException(errorMessage(o, r), r.code());
            }
            return o;
        }
    }

    private JSONObject api(String method, String path, JSONObject body, boolean retried) throws IOException, CloudException {
        if (!CloudDeckApi.isRequestAllowed(method, path)) {
            throw new CloudException("Request not allowed: " + method + " " + path, 0);
        }
        ensureAccessToken();
        Request.Builder b = new Request.Builder()
                .url(CloudDeckApi.API_BASE + path)
                .header("Authorization", "Bearer " + accessToken);
        if (method.equals("POST")) {
            b.post(RequestBody.create((body != null ? body : new JSONObject()).toString(), JSON));
        }
        try (Response r = http.newCall(b.build()).execute()) {
            if (r.code() == 401 && !retried) {
                synchronized (this) { accessToken = null; }
                return api(method, path, body, true);
            }
            JSONObject o = readJson(r);
            if (!r.isSuccessful()) {
                throw new CloudException(errorMessage(o, r), r.code());
            }
            return o;
        }
    }

    // ------------------------------------------------------------------ auth

    private synchronized void applyAuthResult(JSONObject ar) throws CloudException {
        accessToken = ar.optString("AccessToken", "");
        idToken = ar.optString("IdToken", "");
        accessExpiryMs = System.currentTimeMillis() + ar.optInt("ExpiresIn", 3600) * 1000L;
        String rt = ar.optString("RefreshToken", "");
        if (!rt.isEmpty()) {
            refreshToken = rt;
            secrets.write(REFRESH_KEY, rt);
        }
        JSONObject claims = CloudDeckApi.decodeJwtClaims(idToken);
        accountId = claims.optString("custom:account", "");
        String e = claims.optString("email", "");
        if (!e.isEmpty()) {
            email = e;
            prefs.edit().putString("email", e).apply();
        }
        if (accessToken.isEmpty() || accountId.isEmpty()) {
            throw new CloudException("Unexpected sign-in response", 0);
        }
    }

    public SignInResult signIn(String user, String password) throws IOException, CloudException {
        try {
            JSONObject params = new JSONObject().put("USERNAME", user).put("PASSWORD", password);
            JSONObject body = new JSONObject()
                    .put("AuthFlow", "USER_PASSWORD_AUTH")
                    .put("ClientId", CloudDeckApi.COGNITO_CLIENT_ID)
                    .put("AuthParameters", params);
            JSONObject o = cognito("InitiateAuth", body);
            if (o.has("ChallengeName")) {
                synchronized (this) {
                    challengeName = o.getString("ChallengeName");
                    challengeSession = o.optString("Session", "");
                    challengeUser = user;
                }
                if (challengeName.equals("SOFTWARE_TOKEN_MFA") || challengeName.equals("SMS_MFA")) {
                    return SignInResult.MFA_REQUIRED;
                }
                throw new CloudException("Unsupported sign-in challenge: " + challengeName, 0);
            }
            applyAuthResult(o.getJSONObject("AuthenticationResult"));
            return SignInResult.OK;
        } catch (JSONException e) {
            throw new CloudException("Unexpected sign-in response", 0);
        }
    }

    public void respondMfa(String code) throws IOException, CloudException {
        try {
            String codeKey = "SMS_MFA".equals(challengeName) ? "SMS_MFA_CODE" : "SOFTWARE_TOKEN_MFA_CODE";
            JSONObject body = new JSONObject()
                    .put("ChallengeName", challengeName)
                    .put("ClientId", CloudDeckApi.COGNITO_CLIENT_ID)
                    .put("Session", challengeSession)
                    .put("ChallengeResponses", new JSONObject().put("USERNAME", challengeUser).put(codeKey, code));
            JSONObject o = cognito("RespondToAuthChallenge", body);
            applyAuthResult(o.getJSONObject("AuthenticationResult"));
        } catch (JSONException e) {
            throw new CloudException("Unexpected sign-in response", 0);
        } finally {
            synchronized (this) { challengeSession = null; }
        }
    }

    private void refresh() throws IOException, CloudException {
        String rt;
        synchronized (this) { rt = refreshToken; }
        if (rt == null || rt.isEmpty()) {
            throw new CloudException("Not signed in", 0);
        }
        try {
            JSONObject body = new JSONObject()
                    .put("AuthFlow", "REFRESH_TOKEN_AUTH")
                    .put("ClientId", CloudDeckApi.COGNITO_CLIENT_ID)
                    .put("AuthParameters", new JSONObject().put("REFRESH_TOKEN", rt));
            applyAuthResult(cognito("InitiateAuth", body).getJSONObject("AuthenticationResult"));
        } catch (CloudException e) {
            if (e.httpStatus == 400) {
                signOut(); // refresh token expired or revoked
            }
            throw e;
        } catch (JSONException e) {
            throw new CloudException("Unexpected token refresh response", 0);
        }
    }

    private void ensureAccessToken() throws IOException, CloudException {
        synchronized (this) {
            if (accessToken != null && !accessToken.isEmpty() && accessExpiryMs - System.currentTimeMillis() > 60_000) {
                return;
            }
        }
        refresh();
    }

    /** @return false if there was no stored session. */
    public boolean restoreSession() throws IOException, CloudException {
        String rt = secrets.read(REFRESH_KEY);
        if (rt == null || rt.isEmpty()) {
            return false;
        }
        synchronized (this) { refreshToken = rt; }
        refresh();
        return true;
    }

    public synchronized void signOut() {
        accessToken = idToken = refreshToken = null;
        accountId = machineId = null;
        secrets.remove(REFRESH_KEY);
    }

    // ------------------------------------------------------------------ machine

    private String machineId() throws IOException, CloudException {
        synchronized (this) {
            if (machineId != null && !machineId.isEmpty()) return machineId;
        }
        ensureAccessToken();
        JSONObject acct = api("GET", "/accounts/" + accountId, null, false);
        String id = acct.optString("machine_id", "");
        if (id.isEmpty()) {
            throw new CloudException("This account has no CloudDeck machine", 0);
        }
        synchronized (this) { machineId = id; }
        return id;
    }

    public CloudDeckApi.Machine getMachine() throws IOException, CloudException {
        CloudDeckApi.Machine m = CloudDeckApi.parseMachine(api("GET", "/machines/" + machineId(), null, false));
        if (!m.valid) {
            throw new CloudException("Unexpected machine response", 0);
        }
        return m;
    }

    public void startMachine(String sessionMode) throws IOException, CloudException {
        JSONObject body = new JSONObject();
        try {
            if ("desktop".equals(sessionMode) || "gaming".equals(sessionMode)) {
                body.put("session", sessionMode);
            }
        } catch (JSONException ignored) {}
        api("POST", "/machines/" + machineId() + "/start", body, false);
    }

    public void stopMachine() throws IOException, CloudException {
        api("POST", "/machines/" + machineId() + "/stop", null, false);
    }

    public void submitPairingPin(String pin) throws IOException, CloudException {
        try {
            api("POST", "/machines/" + machineId() + "/clients", new JSONObject().put("pin", pin), false);
        } catch (JSONException e) {
            throw new CloudException(e.getMessage(), 0);
        }
    }

    public String hostAddress(CloudDeckApi.Machine m) {
        return CloudDeckApi.hostAddressFor(m.id);
    }
}
