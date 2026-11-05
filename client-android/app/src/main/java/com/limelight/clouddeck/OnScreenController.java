package com.limelight.clouddeck;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import com.limelight.binding.input.virtual_controller.DigitalButton;
import com.limelight.binding.input.virtual_controller.VirtualController;
import com.limelight.binding.input.virtual_controller.VirtualControllerConfigurationLoader;
import com.limelight.binding.input.virtual_controller.VirtualControllerElement;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * CloudDeck-Client: on-screen controller extras (Phase 6, docs/controller-profiles.md):
 * an Xbox or PlayStation button style, and named layout profiles on top of
 * Moonlight's single saved layout.
 */
public final class OnScreenController {
    public static final String STYLE_XBOX = "xbox";
    public static final String STYLE_PLAYSTATION = "playstation";

    private static final String PREF_STYLE = "cdOscButtonStyle";
    private static final String PREF_PROFILES = "cdOscLayoutProfiles";
    private static final String PROFILE_PREFIX = "OSC_profile_";

    private OnScreenController() {}

    private static SharedPreferences prefs(Context c) {
        return PreferenceManager.getDefaultSharedPreferences(c);
    }

    public static String style(Context c) {
        return prefs(c).getString(PREF_STYLE, STYLE_XBOX);
    }

    public static void setStyle(Context c, String style) {
        prefs(c).edit().putString(PREF_STYLE, style).apply();
    }

    /**
     * Relabels and recolours the buttons after a layout was (re)built. Labels follow
     * the button a key sends, so they stay right with the Nintendo layout switch.
     */
    public static void applyStyle(VirtualController controller, Context c, boolean flipFaceButtons) {
        final boolean ps = STYLE_PLAYSTATION.equals(style(c));
        for (VirtualControllerElement e : controller.getElements()) {
            if (!(e instanceof DigitalButton)) continue;
            DigitalButton b = (DigitalButton) e;
            int id = e.getElementId();
            // Which face button this element sends (VirtualControllerConfigurationLoader swaps
            // A<->B and X<->Y when the face buttons are flipped).
            if (flipFaceButtons) {
                if (id == VirtualControllerElement.EID_A) id = VirtualControllerElement.EID_B;
                else if (id == VirtualControllerElement.EID_B) id = VirtualControllerElement.EID_A;
                else if (id == VirtualControllerElement.EID_X) id = VirtualControllerElement.EID_Y;
                else if (id == VirtualControllerElement.EID_Y) id = VirtualControllerElement.EID_X;
            }
            switch (id) {
                case VirtualControllerElement.EID_A:
                    b.setText(ps ? "✕" : "A");
                    b.setLabelColor(ps ? 0xFF7EB6FF : 0xFF5DC21E);
                    break;
                case VirtualControllerElement.EID_B:
                    b.setText(ps ? "○" : "B");
                    b.setLabelColor(ps ? 0xFFFF6B6B : 0xFFE8312F);
                    break;
                case VirtualControllerElement.EID_X:
                    b.setText(ps ? "□" : "X");
                    b.setLabelColor(ps ? 0xFFF38BD6 : 0xFF2D7FE0);
                    break;
                case VirtualControllerElement.EID_Y:
                    b.setText(ps ? "△" : "Y");
                    b.setLabelColor(ps ? 0xFF40E0B0 : 0xFFF4C20D);
                    break;
                case VirtualControllerElement.EID_LB: b.setText(ps ? "L1" : "LB"); break;
                case VirtualControllerElement.EID_RB: b.setText(ps ? "R1" : "RB"); break;
                case VirtualControllerElement.EID_LT: b.setText(ps ? "L2" : "LT"); break;
                case VirtualControllerElement.EID_RT: b.setText(ps ? "R2" : "RT"); break;
                case VirtualControllerElement.EID_BACK: b.setText(ps ? "SHARE" : "VIEW"); break;
                case VirtualControllerElement.EID_START: b.setText(ps ? "OPTIONS" : "MENU"); break;
                case VirtualControllerElement.EID_GDB: b.setText(ps ? "PS" : "GUIDE"); break;
                default: break;
            }
        }
    }

    // ---------------------------------------------------------------- layout profiles

    public static List<String> layoutProfiles(Context c) {
        List<String> l = new ArrayList<>(prefs(c).getStringSet(PREF_PROFILES, Collections.emptySet()));
        Collections.sort(l, String.CASE_INSENSITIVE_ORDER);
        return l;
    }

    /** Saves the current layout (positions, sizes, colours) under a name. */
    public static void saveLayout(VirtualController controller, Context c, String name) {
        VirtualControllerConfigurationLoader.saveProfile(controller, c); // current state -> "OSC"
        copy(c.getSharedPreferences(VirtualControllerConfigurationLoader.OSC_PREFERENCE, Activity.MODE_PRIVATE),
                c.getSharedPreferences(PROFILE_PREFIX + name, Activity.MODE_PRIVATE));
        Set<String> names = new HashSet<>(prefs(c).getStringSet(PREF_PROFILES, Collections.emptySet()));
        names.add(name);
        prefs(c).edit().putStringSet(PREF_PROFILES, names).apply();
    }

    /** Makes a saved layout the current one and rebuilds the controller. */
    public static void loadLayout(VirtualController controller, Context c, String name) {
        copy(c.getSharedPreferences(PROFILE_PREFIX + name, Activity.MODE_PRIVATE),
                c.getSharedPreferences(VirtualControllerConfigurationLoader.OSC_PREFERENCE, Activity.MODE_PRIVATE));
        controller.refreshLayout();
    }

    public static void deleteLayout(Context c, String name) {
        c.getSharedPreferences(PROFILE_PREFIX + name, Activity.MODE_PRIVATE).edit().clear().apply();
        Set<String> names = new HashSet<>(prefs(c).getStringSet(PREF_PROFILES, Collections.emptySet()));
        names.remove(name);
        prefs(c).edit().putStringSet(PREF_PROFILES, names).apply();
    }

    private static void copy(SharedPreferences from, SharedPreferences to) {
        SharedPreferences.Editor e = to.edit().clear();
        for (Map.Entry<String, ?> entry : from.getAll().entrySet()) {
            if (entry.getValue() instanceof String) e.putString(entry.getKey(), (String) entry.getValue());
        }
        e.commit(); // synchronous: the layout is rebuilt right after a load
    }
}
