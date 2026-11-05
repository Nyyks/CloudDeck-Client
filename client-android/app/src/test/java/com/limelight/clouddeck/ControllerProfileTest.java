package com.limelight.clouddeck;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.Test;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** Same fixture and expectations as the desktop client (shared/fixtures/controller). */
public class ControllerProfileTest {
    private static JSONObject fixture(String name) throws Exception {
        String dir = System.getProperty("fixtures.dir", "../../shared/fixtures/clouddeck");
        byte[] b = Files.readAllBytes(new File(new File(dir).getParentFile(), "controller/" + name).toPath());
        return new JSONObject(new String(b, StandardCharsets.UTF_8));
    }

    @Test
    public void sharedFixture() throws Exception {
        ControllerProfile p = ControllerProfile.fromJson(fixture("profile-v1.json"));
        JSONObject cases = fixture("cases-v1.json");
        assertEquals("Test profile", p.name);
        assertFalse(p.buttons.containsKey("bogus"));
        assertFalse(p.buttons.containsKey("x"));

        JSONArray buttons = cases.getJSONArray("buttons");
        for (int i = 0; i < buttons.length(); i++) {
            JSONArray c = buttons.getJSONArray(i);
            assertEquals(c.getString(1), p.mapButton(c.getString(0)));
        }
        JSONArray masks = cases.getJSONArray("masks");
        for (int i = 0; i < masks.length(); i++) {
            JSONArray c = masks.getJSONArray(i);
            assertEquals("mask " + c.getInt(0), c.getInt(1), p.mapButtonMask(c.getInt(0)));
        }
        JSONArray sticks = cases.getJSONArray("sticks");
        for (int i = 0; i < sticks.length(); i++) {
            JSONArray in = sticks.getJSONObject(i).getJSONArray("in"), out = sticks.getJSONObject(i).getJSONArray("out");
            short[] s = {(short) in.getInt(0), (short) in.getInt(1), (short) in.getInt(2), (short) in.getInt(3)};
            p.applySticks(s);
            assertArrayEquals(new short[]{(short) out.getInt(0), (short) out.getInt(1), (short) out.getInt(2), (short) out.getInt(3)}, s);
        }
        JSONArray triggers = cases.getJSONArray("triggers");
        for (int i = 0; i < triggers.length(); i++) {
            JSONObject c = triggers.getJSONObject(i);
            assertEquals(c.getInt("out"), p.applyTrigger(c.getInt("in")));
        }

        ControllerProfile again = ControllerProfile.fromJson(p.toJson());
        assertEquals(p.buttons, again.buttons);
        assertEquals(2.0, again.rightStick.sensitivity, 0);
        assertTrue(again.rightStick.invertY);
        assertFalse(p.isIdentity());
        assertTrue(new ControllerProfile().isIdentity());

        ControllerProfile sw = new ControllerProfile();
        sw.swapSticks = true;
        short[] s = {100, 200, 300, 400};
        sw.applySticks(s);
        assertArrayEquals(new short[]{300, 400, 100, 200}, s);
    }
}
