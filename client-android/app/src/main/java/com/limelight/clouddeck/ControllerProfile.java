package com.limelight.clouddeck;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * CloudDeck-Client: controller remapping profile (docs/controller-profiles.md).
 * The same format and rules as the desktop client's ControllerProfile, checked by
 * both clients' tests against shared/fixtures/controller.
 */
public final class ControllerProfile {
    public static final List<String> BUTTONS = Collections.unmodifiableList(Arrays.asList(
            "a", "b", "x", "y", "back", "guide", "start", "leftStick", "rightStick", "leftShoulder", "rightShoulder",
            "dpadUp", "dpadDown", "dpadLeft", "dpadRight", "misc", "paddle1", "paddle2", "paddle3", "paddle4", "touchpad"));

    // Moonlight button flags (ControllerPacket / Limelight.h), in BUTTONS order.
    private static final int[] FLAGS = {
            0x1000, 0x2000, 0x4000, 0x8000, 0x0020, 0x0400, 0x0010, 0x0040, 0x0080, 0x0100, 0x0200,
            0x0001, 0x0002, 0x0004, 0x0008, 0x200000, 0x010000, 0x020000, 0x040000, 0x080000, 0x100000};

    public static final class Stick {
        public double deadzone = 0.0;
        public double sensitivity = 1.0;
        public boolean invertY = false;
    }

    public String name = "Default";
    public final Map<String, String> buttons = new LinkedHashMap<>(); // source -> target ("none" = disabled)
    public Stick leftStick = new Stick(), rightStick = new Stick();
    public double triggerDeadzone = 0.0;
    public boolean swapSticks = false;

    private static double clamp(double v, double lo, double hi) { return Math.max(lo, Math.min(hi, v)); }

    // Half away from zero, like C++ lround (Java's Math.round rounds -2.5 to -2).
    private static long roundAway(double v) { return v < 0 ? -Math.round(-v) : Math.round(v); }

    private static Stick stickFromJson(JSONObject o) {
        Stick s = new Stick();
        if (o == null) return s;
        s.deadzone = clamp(o.optDouble("deadzone", 0.0), 0.0, 0.5);
        s.sensitivity = clamp(o.optDouble("sensitivity", 1.0), 0.5, 2.0);
        s.invertY = o.optBoolean("invertY", false);
        return s;
    }

    public static ControllerProfile fromJson(JSONObject o) {
        ControllerProfile p = new ControllerProfile();
        p.name = o.optString("name", p.name);
        JSONObject b = o.optJSONObject("buttons");
        if (b != null) {
            for (Iterator<String> it = b.keys(); it.hasNext(); ) {
                String src = it.next();
                String target = b.optString(src, "");
                if (BUTTONS.contains(src) && (target.equals("none") || BUTTONS.contains(target))) {
                    p.buttons.put(src, target);
                }
            }
        }
        p.leftStick = stickFromJson(o.optJSONObject("leftStick"));
        p.rightStick = stickFromJson(o.optJSONObject("rightStick"));
        p.triggerDeadzone = clamp(o.optDouble("triggerDeadzone", 0.0), 0.0, 0.5);
        p.swapSticks = o.optBoolean("swapSticks", false);
        return p;
    }

    private static JSONObject stickToJson(Stick s) throws JSONException {
        return new JSONObject().put("deadzone", s.deadzone).put("sensitivity", s.sensitivity).put("invertY", s.invertY);
    }

    public JSONObject toJson() {
        try {
            JSONObject b = new JSONObject();
            for (Map.Entry<String, String> e : buttons.entrySet()) {
                if (!e.getKey().equals(e.getValue())) b.put(e.getKey(), e.getValue());
            }
            return new JSONObject().put("version", 1).put("name", name).put("buttons", b)
                    .put("leftStick", stickToJson(leftStick)).put("rightStick", stickToJson(rightStick))
                    .put("triggerDeadzone", triggerDeadzone).put("swapSticks", swapSticks);
        } catch (JSONException e) {
            throw new IllegalStateException(e);
        }
    }

    public boolean isIdentity() {
        for (Map.Entry<String, String> e : buttons.entrySet()) {
            if (!e.getKey().equals(e.getValue())) return false;
        }
        return plain(leftStick) && plain(rightStick) && triggerDeadzone == 0 && !swapSticks;
    }

    private static boolean plain(Stick s) { return s.deadzone == 0 && s.sensitivity == 1 && !s.invertY; }

    public String mapButton(String source) {
        String t = buttons.get(source);
        return t != null ? t : source;
    }

    public static int buttonFlag(String name) {
        int i = BUTTONS.indexOf(name);
        return i >= 0 ? FLAGS[i] : 0;
    }

    /** The final button bitmask after remapping; unknown bits pass through. */
    public int mapButtonMask(int mask) {
        if (buttons.isEmpty()) return mask;
        int out = 0, known = 0;
        for (int i = 0; i < BUTTONS.size(); i++) {
            known |= FLAGS[i];
            if ((mask & FLAGS[i]) != 0) out |= buttonFlag(mapButton(BUTTONS.get(i)));
        }
        return out | (mask & ~known);
    }

    private static short[] applyStick(Stick s, short x, short y) {
        double fx = x / 32767.0, fy = y / 32767.0;
        double mag = Math.min(1.0, Math.sqrt(fx * fx + fy * fy));
        double out = 0;
        if (mag > s.deadzone && mag > 0) {
            out = Math.pow((mag - s.deadzone) / (1.0 - s.deadzone), 1.0 / s.sensitivity);
        }
        double nx = mag > 0 ? fx / mag * out : 0, ny = mag > 0 ? fy / mag * out : 0;
        if (s.invertY) ny = -ny;
        return new short[]{(short) roundAway(clamp(nx, -1, 1) * 32767), (short) roundAway(clamp(ny, -1, 1) * 32767)};
    }

    /** sticks = {lx, ly, rx, ry} in Moonlight's convention (+Y up), changed in place. */
    public void applySticks(short[] sticks) {
        short[] l = applyStick(leftStick, sticks[0], sticks[1]);
        short[] r = applyStick(rightStick, sticks[2], sticks[3]);
        if (swapSticks) { short[] t = l; l = r; r = t; }
        sticks[0] = l[0]; sticks[1] = l[1]; sticks[2] = r[0]; sticks[3] = r[1];
    }

    /** Trigger 0..255. */
    public int applyTrigger(int value) {
        double v = value / 255.0;
        if (v <= triggerDeadzone) return 0;
        return (int) roundAway((v - triggerDeadzone) / (1.0 - triggerDeadzone) * 255);
    }

    // ---------------------------------------------------------------- store

    public static final class Store {
        private static final String PROFILES = "cdControllerProfiles";
        private static final String DEFAULT = "cdControllerDefaultProfile";
        private static final String ASSIGN = "cdControllerAssignments";

        private static SharedPreferences prefs(Context c) {
            return PreferenceManager.getDefaultSharedPreferences(c);
        }

        public static List<ControllerProfile> profiles(Context c) {
            List<ControllerProfile> out = new ArrayList<>();
            try {
                JSONArray a = new JSONArray(prefs(c).getString(PROFILES, "[]"));
                for (int i = 0; i < a.length(); i++) out.add(fromJson(a.getJSONObject(i)));
            } catch (JSONException ignored) {
            }
            return out;
        }

        public static void saveProfiles(Context c, List<ControllerProfile> list) {
            JSONArray a = new JSONArray();
            for (ControllerProfile p : list) a.put(p.toJson());
            prefs(c).edit().putString(PROFILES, a.toString()).apply();
        }

        public static String defaultProfile(Context c) { return prefs(c).getString(DEFAULT, ""); }

        public static void setDefaultProfile(Context c, String name) { prefs(c).edit().putString(DEFAULT, name).apply(); }

        public static Map<String, String> assignments(Context c) {
            Map<String, String> out = new HashMap<>();
            try {
                JSONObject o = new JSONObject(prefs(c).getString(ASSIGN, "{}"));
                for (Iterator<String> it = o.keys(); it.hasNext(); ) {
                    String k = it.next();
                    out.put(k, o.getString(k));
                }
            } catch (JSONException ignored) {
            }
            return out;
        }

        public static void setAssignment(Context c, String controller, String profile) {
            Map<String, String> a = assignments(c);
            if (profile == null || profile.isEmpty()) a.remove(controller);
            else a.put(controller, profile);
            prefs(c).edit().putString(ASSIGN, new JSONObject(a).toString()).apply();
        }

        /** The profile for a controller name, or null for no remapping. */
        public static ControllerProfile profileFor(Context c, String controllerName) {
            String name = assignments(c).get(controllerName);
            if (name == null) name = defaultProfile(c);
            for (ControllerProfile p : profiles(c)) {
                if (p.name.equals(name)) return p.isIdentity() ? null : p;
            }
            return null;
        }
    }
}
