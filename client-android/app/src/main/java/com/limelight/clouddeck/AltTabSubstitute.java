package com.limelight.clouddeck;

import android.view.KeyEvent;

import com.limelight.nvstream.input.KeyboardPacket;

/**
 * CloudDeck-Client: a stand-in shortcut for Alt+Tab (docs/keyboard-capture.md).
 *
 * Android before 16 QPR1 keeps Alt+Tab for itself: PhoneWindowManager consumes the first Tab
 * pressed with Alt (it opens the system's recents, on ChromeOS the window switcher), then also the
 * Alt release. No app sees them, accessibility key filters included. So another shortcut is sent
 * to the CloudDeck as Alt+Tab instead:
 *  - Ctrl+Tab (default): the first Tab with Ctrl held releases Ctrl on the CloudDeck and holds Alt
 *    there instead, until Ctrl comes up. Each Tab meanwhile is a Tab, so holding Ctrl and tapping
 *    Tab moves through the windows like Alt+Tab, and Shift goes backwards.
 *  - Alt+` (the key above Tab): with Alt held, ` is sent as Tab.
 * Pure logic, no Android calls: unit-tested in AltTabSubstituteTest.
 */
public final class AltTabSubstitute {
    public static final String PREF = "cdAltTabSubstitute";
    public static final String OFF = "off";
    public static final String CTRL_TAB = "ctrl_tab";
    public static final String ALT_GRAVE = "alt_grave";
    public static final String DEFAULT = CTRL_TAB;

    static final short VK_TAB = 0x09;
    static final short VK_LCONTROL = 0xA2;
    static final short VK_RCONTROL = 0xA3;
    static final short VK_LMENU = 0xA4;

    /** Sends one key to the CloudDeck (Windows virtual key, KeyboardPacket modifier flags). */
    public interface Sender {
        void key(short vk, boolean down, byte modifiers);
    }

    private final String mode;
    private final Sender out;
    private boolean holdingAlt; // Ctrl+Tab: the CloudDeck holds Alt for us until Ctrl comes up
    private boolean tabDown;    // our Tab is down on the CloudDeck

    public AltTabSubstitute(String mode, Sender out) {
        this.mode = mode == null ? DEFAULT : mode;
        this.out = out;
    }

    public static String normalize(String mode) {
        return OFF.equals(mode) || ALT_GRAVE.equals(mode) ? mode : CTRL_TAB;
    }

    public String mode() { return mode; }

    /** True if the shortcut is on. */
    public boolean enabled() { return !OFF.equals(mode); }

    /**
     * One key from the keyboard while the stream has the input. Returns true if it was handled
     * here and must not be sent as usual.
     *
     * @param metaState KeyEvent.getMetaState() of the event (after the key changed it)
     */
    public boolean onKey(int keyCode, boolean down, int repeatCount, int metaState) {
        if (CTRL_TAB.equals(mode)) return ctrlTab(keyCode, down, repeatCount, metaState);
        if (ALT_GRAVE.equals(mode)) return altGrave(keyCode, down, repeatCount, metaState);
        return false;
    }

    private boolean ctrlTab(int keyCode, boolean down, int repeatCount, int metaState) {
        final boolean ctrl = (metaState & KeyEvent.META_CTRL_ON) != 0;
        final boolean alt = (metaState & KeyEvent.META_ALT_ON) != 0;
        final boolean meta = (metaState & KeyEvent.META_META_ON) != 0;
        final byte shift = (metaState & KeyEvent.META_SHIFT_ON) != 0 ? KeyboardPacket.MODIFIER_SHIFT : 0;

        if (keyCode == KeyEvent.KEYCODE_TAB) {
            if (down) {
                if (!holdingAlt && !(ctrl && !alt && !meta)) return false; // a plain Tab
                if (repeatCount > 0) return true; // the CloudDeck repeats on its own
                if (!holdingAlt) {
                    holdingAlt = true;
                    // Ctrl is already down on the CloudDeck: swap it for Alt.
                    if ((metaState & KeyEvent.META_CTRL_LEFT_ON) != 0) out.key(VK_LCONTROL, false, shift);
                    if ((metaState & KeyEvent.META_CTRL_RIGHT_ON) != 0) out.key(VK_RCONTROL, false, shift);
                    out.key(VK_LMENU, true, (byte) (KeyboardPacket.MODIFIER_ALT | shift));
                }
                out.key(VK_TAB, true, (byte) (KeyboardPacket.MODIFIER_ALT | shift));
                tabDown = true;
                return true;
            }
            if (!tabDown) return false;
            tabDown = false;
            out.key(VK_TAB, false, (byte) (KeyboardPacket.MODIFIER_ALT | shift));
            return true;
        }

        if (holdingAlt && (keyCode == KeyEvent.KEYCODE_CTRL_LEFT || keyCode == KeyEvent.KEYCODE_CTRL_RIGHT)) {
            // Ctrl stands for Alt now: its presses and releases don't reach the CloudDeck as Ctrl.
            if (!down && !ctrl) {
                releaseTab(shift);
                out.key(VK_LMENU, false, shift);
                holdingAlt = false;
            }
            return true;
        }
        return false;
    }

    private boolean altGrave(int keyCode, boolean down, int repeatCount, int metaState) {
        if (keyCode != KeyEvent.KEYCODE_GRAVE) return false;
        final boolean ctrl = (metaState & KeyEvent.META_CTRL_ON) != 0;
        final boolean alt = (metaState & KeyEvent.META_ALT_ON) != 0;
        final boolean meta = (metaState & KeyEvent.META_META_ON) != 0;
        final byte shift = (metaState & KeyEvent.META_SHIFT_ON) != 0 ? KeyboardPacket.MODIFIER_SHIFT : 0;
        if (down) {
            if (!(alt && !ctrl && !meta)) return false; // a plain `
            if (repeatCount > 0) return true;
            out.key(VK_TAB, true, (byte) (KeyboardPacket.MODIFIER_ALT | shift));
            tabDown = true;
            return true;
        }
        if (!tabDown) return false;
        releaseTab(shift);
        return true;
    }

    private void releaseTab(byte shift) {
        if (!tabDown) return;
        tabDown = false;
        out.key(VK_TAB, false, (byte) (KeyboardPacket.MODIFIER_ALT | shift));
    }

    /** Focus lost or the stream lets go of the input: release what the CloudDeck holds for us. */
    public void reset() {
        releaseTab((byte) 0);
        if (holdingAlt) {
            holdingAlt = false;
            out.key(VK_LMENU, false, (byte) 0);
        }
    }
}
