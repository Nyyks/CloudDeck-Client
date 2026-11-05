package com.limelight.clouddeck;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

/**
 * CloudDeck-Client: full on-screen keyboard for phones, tablets and handhelds
 * (Phase 6, docs/on-screen-keyboard.md). Unlike the system keyboard it sends real
 * key presses, so Esc, F-keys, arrows and shortcuts such as Ctrl+C work in games
 * and on the desktop. Modifiers latch for one key (tap) or lock (double tap).
 */
public final class StreamKeyboard {
    /** Sends one key event to the CloudDeck (Windows virtual-key code). */
    public interface KeySender {
        void send(int vk, boolean down, byte modifiers);
    }

    private static final int VK_SHIFT = 0xA0, VK_CTRL = 0xA2, VK_ALT = 0xA4, VK_WIN = 0x5B;
    private static final byte MOD_SHIFT = 0x01, MOD_CTRL = 0x02, MOD_ALT = 0x04, MOD_META = 0x08;
    private static final int CLOSE = -1;

    private static final class Key {
        final String label; final int vk; final float weight; final byte modifier;
        Key(String label, int vk, float weight) { this(label, vk, weight, (byte) 0); }
        Key(String label, int vk, float weight, byte modifier) {
            this.label = label; this.vk = vk; this.weight = weight; this.modifier = modifier;
        }
    }

    private static Key k(String l, int vk) { return new Key(l, vk, 1f); }

    // US layout, the same positions as a physical keyboard.
    private static final Key[][] ROWS = {
            {new Key("Esc", 27, 1.2f), k("F1", 112), k("F2", 113), k("F3", 114), k("F4", 115), k("F5", 116), k("F6", 117),
                    k("F7", 118), k("F8", 119), k("F9", 120), k("F10", 121), k("F11", 122), k("F12", 123), new Key("Del", 46, 1.2f)},
            {k("`", 0xC0), k("1", 49), k("2", 50), k("3", 51), k("4", 52), k("5", 53), k("6", 54), k("7", 55), k("8", 56),
                    k("9", 57), k("0", 48), k("-", 0xBD), k("=", 0xBB), new Key("⌫", 8, 1.6f)},
            {new Key("Tab", 9, 1.5f), k("Q", 81), k("W", 87), k("E", 69), k("R", 82), k("T", 84), k("Y", 89), k("U", 85),
                    k("I", 73), k("O", 79), k("P", 80), k("[", 0xDB), k("]", 0xDD), new Key("\\", 0xDC, 1.1f)},
            {new Key("Caps", 20, 1.75f), k("A", 65), k("S", 83), k("D", 68), k("F", 70), k("G", 71), k("H", 72), k("J", 74),
                    k("K", 75), k("L", 76), k(";", 0xBA), k("'", 0xDE), new Key("Enter", 13, 1.85f)},
            {new Key("Shift", VK_SHIFT, 2.2f, MOD_SHIFT), k("Z", 90), k("X", 88), k("C", 67), k("V", 86), k("B", 66), k("N", 78),
                    k("M", 77), k(",", 0xBC), k(".", 0xBE), k("/", 0xBF), k("↑", 38), new Key("PgUp", 33, 1.2f)},
            {new Key("Ctrl", VK_CTRL, 1.4f, MOD_CTRL), new Key("Win", VK_WIN, 1.2f, MOD_META), new Key("Alt", VK_ALT, 1.2f, MOD_ALT),
                    new Key("", 32, 5f), k("←", 37), k("↓", 40), k("→", 39), new Key("PgDn", 34, 1.2f),
                    new Key("✕", CLOSE, 1.2f)},
    };

    private final Activity activity;
    private final KeySender sender;
    private final LinearLayout panel;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final List<TextView> modifierViews = new ArrayList<>();
    private byte latched = 0, locked = 0;
    private long lastModifierTap = 0;
    private byte lastModifier = 0;

    public StreamKeyboard(Activity activity, KeySender sender) {
        this.activity = activity;
        this.sender = sender;

        panel = new LinearLayout(activity);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setBackgroundColor(Color.parseColor("#E61D222B"));
        int pad = dp(4);
        panel.setPadding(pad, pad, pad, pad);
        panel.setVisibility(View.GONE);
        panel.setClickable(true); // touches on the panel never reach the stream

        for (Key[] row : ROWS) {
            LinearLayout r = new LinearLayout(activity);
            r.setOrientation(LinearLayout.HORIZONTAL);
            for (Key key : row) r.addView(keyView(key), new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, key.weight));
            panel.addView(r, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        }

        int height = Math.round(activity.getResources().getDisplayMetrics().heightPixels * 0.46f);
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height, Gravity.BOTTOM);
        ((ViewGroup) activity.findViewById(android.R.id.content)).addView(panel, lp);
    }

    public boolean isShowing() { return panel.getVisibility() == View.VISIBLE; }

    public void show() {
        panel.setVisibility(View.VISIBLE);
        panel.bringToFront();
    }

    public void hide() {
        releaseModifiers();
        panel.setVisibility(View.GONE);
    }

    public void toggle() { if (isShowing()) hide(); else show(); }

    @SuppressLint("ClickableViewAccessibility")
    private View keyView(final Key key) {
        final TextView v = new TextView(activity);
        v.setText(key.label);
        v.setGravity(Gravity.CENTER);
        v.setTextColor(Color.WHITE);
        v.setTextSize(TypedValue.COMPLEX_UNIT_SP, key.label.length() > 2 ? 12 : 16);
        v.setBackground(keyBackground(false));
        ViewGroup.MarginLayoutParams m = new ViewGroup.MarginLayoutParams(0, 0);
        v.setLayoutParams(m);
        if (key.modifier != 0) modifierViews.add(v);
        v.setTag(key);

        v.setOnTouchListener(new View.OnTouchListener() {
            private Runnable repeat;

            @Override
            public boolean onTouch(View view, MotionEvent e) {
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        view.setBackground(keyBackground(true));
                        if (key.vk == CLOSE) return true;
                        if (key.modifier != 0) { tapModifier(key.modifier); return true; }
                        keyDown(key.vk);
                        // Held keys repeat like a physical keyboard.
                        repeat = new Runnable() {
                            @Override public void run() {
                                sender.send(key.vk, true, currentModifiers());
                                handler.postDelayed(this, 50);
                            }
                        };
                        handler.postDelayed(repeat, 450);
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        view.setBackground(keyBackground(false));
                        if (repeat != null) { handler.removeCallbacks(repeat); repeat = null; }
                        if (key.vk == CLOSE) { if (e.getActionMasked() == MotionEvent.ACTION_UP) hide(); return true; }
                        if (key.modifier != 0) { refreshModifiers(); return true; }
                        keyUp(key.vk);
                        return true;
                }
                return true;
            }
        });
        return v;
    }

    private byte currentModifiers() { return (byte) (latched | locked); }

    private void tapModifier(byte mod) {
        long now = System.currentTimeMillis();
        if ((locked & mod) != 0) {
            locked &= ~mod; // tap a locked modifier: unlock
            latched &= ~mod;
            sendModifier(mod, false);
        } else if (mod == lastModifier && now - lastModifierTap < 400) {
            locked |= mod;  // double tap: lock
            latched &= ~mod;
        } else if ((latched & mod) != 0) {
            latched &= ~mod; // tap a latched modifier again: cancel
            sendModifier(mod, false);
        } else {
            latched |= mod;  // single tap: for the next key
            sendModifier(mod, true);
        }
        lastModifier = mod;
        lastModifierTap = now;
        refreshModifiers();
    }

    private void keyDown(int vk) {
        sender.send(vk, true, currentModifiers());
    }

    private void keyUp(int vk) {
        sender.send(vk, false, currentModifiers());
        // A latched modifier is used up by one key; locked ones stay.
        byte used = (byte) (latched & ~locked);
        for (byte mod : new byte[]{MOD_SHIFT, MOD_CTRL, MOD_ALT, MOD_META}) {
            if ((used & mod) != 0) sendModifier(mod, false);
        }
        latched = 0;
        refreshModifiers();
    }

    private void releaseModifiers() {
        for (byte mod : new byte[]{MOD_SHIFT, MOD_CTRL, MOD_ALT, MOD_META}) {
            if (((latched | locked) & mod) != 0) sendModifier(mod, false);
        }
        latched = locked = 0;
        refreshModifiers();
    }

    private void sendModifier(byte mod, boolean down) {
        int vk = mod == MOD_SHIFT ? VK_SHIFT : mod == MOD_CTRL ? VK_CTRL : mod == MOD_ALT ? VK_ALT : VK_WIN;
        sender.send(vk, down, down ? (byte) (currentModifiers() | mod) : (byte) (currentModifiers() & ~mod));
    }

    private void refreshModifiers() {
        for (TextView v : modifierViews) {
            byte mod = ((Key) v.getTag()).modifier;
            GradientDrawable bg = keyBackground(false);
            if ((locked & mod) != 0) bg.setColor(Color.parseColor("#3B5BDB"));
            else if ((latched & mod) != 0) bg.setColor(Color.parseColor("#2F4A9E"));
            v.setBackground(bg);
        }
    }

    private GradientDrawable keyBackground(boolean pressed) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(Color.parseColor(pressed ? "#4DABF7" : "#2B3240"));
        d.setCornerRadius(dp(6));
        d.setStroke(dp(2), Color.parseColor("#1D222B")); // gap between keys
        return d;
    }

    private int dp(int v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, activity.getResources().getDisplayMetrics()));
    }
}
