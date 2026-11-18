package com.limelight.clouddeck;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.KeyEvent;

import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

public class AltTabSubstituteTest {
    private static final int CTRL_L = KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_LEFT_ON;
    private static final int ALT_L = KeyEvent.META_ALT_ON | KeyEvent.META_ALT_LEFT_ON;
    private static final int SHIFT_L = KeyEvent.META_SHIFT_ON | KeyEvent.META_SHIFT_LEFT_ON;

    private final List<String> sent = new ArrayList<>();

    private AltTabSubstitute make(String mode) {
        return new AltTabSubstitute(mode, (vk, down, mods) ->
                sent.add(String.format("%02X%s/%d", vk, down ? "+" : "-", mods)));
    }

    @Test
    public void ctrlTabHoldsAltWhileCtrlIsDown() {
        AltTabSubstitute s = make(AltTabSubstitute.CTRL_TAB);
        // Ctrl down reaches the CloudDeck as usual (not handled here)
        assertFalse(s.onKey(KeyEvent.KEYCODE_CTRL_LEFT, true, 0, CTRL_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_TAB, false, 0, CTRL_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_TAB, true, 1, CTRL_L)); // auto-repeat: eaten
        assertTrue(s.onKey(KeyEvent.KEYCODE_TAB, false, 0, CTRL_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_CTRL_LEFT, false, 0, 0));
        assertEquals(List.of("A2-/0", "A4+/4", "09+/4", "09-/4", "09+/4", "09-/4", "A4-/0"), sent);
    }

    @Test
    public void ctrlShiftTabGoesBackwards() {
        AltTabSubstitute s = make(AltTabSubstitute.CTRL_TAB);
        assertTrue(s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L | SHIFT_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_TAB, false, 0, CTRL_L | SHIFT_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_CTRL_LEFT, false, 0, SHIFT_L));
        assertEquals(List.of("A2-/1", "A4+/5", "09+/5", "09-/5", "A4-/1"), sent);
    }

    @Test
    public void releasingCtrlBeforeTabStillReleasesEverything() {
        AltTabSubstitute s = make(AltTabSubstitute.CTRL_TAB);
        s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L);
        assertTrue(s.onKey(KeyEvent.KEYCODE_CTRL_LEFT, false, 0, 0));
        assertFalse(s.onKey(KeyEvent.KEYCODE_TAB, false, 0, 0)); // already released
        assertEquals(List.of("A2-/0", "A4+/4", "09+/4", "09-/4", "A4-/0"), sent);
    }

    @Test
    public void otherCombosPassThrough() {
        AltTabSubstitute s = make(AltTabSubstitute.CTRL_TAB);
        assertFalse(s.onKey(KeyEvent.KEYCODE_TAB, true, 0, 0));
        assertFalse(s.onKey(KeyEvent.KEYCODE_TAB, false, 0, 0));
        assertFalse(s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L | ALT_L)); // Ctrl+Alt+Tab
        assertFalse(s.onKey(KeyEvent.KEYCODE_TAB, false, 0, CTRL_L | ALT_L));
        assertFalse(s.onKey(KeyEvent.KEYCODE_C, true, 0, CTRL_L));
        assertFalse(s.onKey(KeyEvent.KEYCODE_CTRL_LEFT, false, 0, 0));
        assertTrue(sent.isEmpty());
    }

    @Test
    public void resetReleasesHeldKeys() {
        AltTabSubstitute s = make(AltTabSubstitute.CTRL_TAB);
        s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L);
        s.reset();
        assertEquals(List.of("A2-/0", "A4+/4", "09+/4", "09-/4", "A4-/0"), sent);
        sent.clear();
        s.reset();
        assertTrue(sent.isEmpty());
    }

    @Test
    public void altGraveSendsTab() {
        AltTabSubstitute s = make(AltTabSubstitute.ALT_GRAVE);
        assertFalse(s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L)); // Ctrl+Tab untouched
        assertFalse(s.onKey(KeyEvent.KEYCODE_GRAVE, true, 0, 0));    // plain `
        assertFalse(s.onKey(KeyEvent.KEYCODE_GRAVE, false, 0, 0));
        assertTrue(s.onKey(KeyEvent.KEYCODE_GRAVE, true, 0, ALT_L));
        assertTrue(s.onKey(KeyEvent.KEYCODE_GRAVE, false, 0, ALT_L));
        assertEquals(List.of("09+/4", "09-/4"), sent);
    }

    @Test
    public void offDoesNothing() {
        AltTabSubstitute s = make(AltTabSubstitute.OFF);
        assertFalse(s.enabled());
        assertFalse(s.onKey(KeyEvent.KEYCODE_TAB, true, 0, CTRL_L));
        assertTrue(sent.isEmpty());
        assertEquals(AltTabSubstitute.CTRL_TAB, AltTabSubstitute.normalize(null));
        assertEquals(AltTabSubstitute.CTRL_TAB, AltTabSubstitute.normalize("bogus"));
    }
}
