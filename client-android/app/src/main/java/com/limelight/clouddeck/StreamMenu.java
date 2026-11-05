package com.limelight.clouddeck;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.preference.PreferenceManager;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;

import com.limelight.R;

import java.util.ArrayList;
import java.util.List;

/**
 * CloudDeck-Client: in-stream menu on Android (docs/adr/0009-quick-menu.md, rev. 3).
 * <p>
 * A small round button at the top centre (the CloudDeck mark), the Back gesture/button
 * and Select+L1+R1+B open a panel: quick toggle tiles (microphone, stats, keyboards,
 * controller as mouse, Nintendo layout), the on-screen controller, then disconnect and
 * shut down. Same look as the home screen (clouddeck_styles.xml); every control takes
 * D-pad focus with a visible ring.
 */
public final class StreamMenu {
    /** What the menu needs from the stream activity (implemented by Game). */
    public interface Host {
        boolean isPerfOverlayVisible();
        void setPerfOverlayVisible(boolean visible);
        boolean isNintendoLayout();
        void setNintendoLayout(boolean on);
        void toggleKeyboard();
        void toggleFullKeyboard();
        void disconnect();
        String streamInfo();
        /** The on-screen controller, or null when it is off. */
        com.limelight.binding.input.virtual_controller.VirtualController virtualController();
        // Controller mouse mode (docs/controller-mouse.md)
        default boolean hasControllers() { return false; }
        default boolean isControllerMouse() { return false; }
        default void setControllerMouse(boolean on) {}
    }

    private static final String PREF_HIDE_BUTTON = "cdMenuButtonHidden";

    private final Activity activity;
    private final Host host;
    private final ImageButton button;
    private AlertDialog dialog;
    private boolean confirmShutdown;

    public StreamMenu(Activity activity, Host host) {
        this.activity = activity;
        this.host = host;

        button = new ImageButton(activity);
        button.setImageResource(R.drawable.cd_mark);
        button.setBackgroundResource(R.drawable.cd_menu_button_bg);
        button.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        int pad = dp(10);
        button.setPadding(pad, pad, pad, pad);
        button.setAlpha(0.8f);
        button.setContentDescription(activity.getString(R.string.cd_menu_button));
        button.setFocusable(false); // keep gamepad/keyboard focus on the stream
        button.setOnClickListener(v -> show());

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(dp(40), dp(40), Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        lp.topMargin = dp(8);
        ((ViewGroup) activity.findViewById(android.R.id.content)).addView(button, lp);
        updateButton();
    }

    /** Hide while overlays are hidden (e.g. picture-in-picture). */
    public void setOverlaysHidden(boolean hidden) {
        button.setVisibility(hidden || buttonHidden() ? View.GONE : View.VISIBLE);
    }

    public boolean isShowing() {
        return dialog != null && dialog.isShowing();
    }

    public void show() {
        if (isShowing()) return;
        confirmShutdown = false;
        dialog = new AlertDialog.Builder(activity, android.R.style.Theme_DeviceDefault_Dialog_NoActionBar)
                .setView(buildContent())
                .create();
        dialog.setCanceledOnTouchOutside(true);
        dialog.show();
        if (dialog.getWindow() != null) {
            GradientDrawable bg = new GradientDrawable();
            bg.setColor(color(R.color.cd_bg));
            bg.setStroke(dp(1), color(R.color.cd_border));
            bg.setCornerRadius(dp(16));
            dialog.getWindow().setBackgroundDrawable(bg);
            int width = Math.min(activity.getResources().getDisplayMetrics().widthPixels - dp(24), dp(440));
            dialog.getWindow().setLayout(width, ViewGroup.LayoutParams.WRAP_CONTENT);
        }
    }

    private View buildContent() {
        LinearLayout root = new LinearLayout(activity);
        root.setOrientation(LinearLayout.VERTICAL);

        // ---- header: mark, name, stream info, close
        LinearLayout header = new LinearLayout(activity);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setPadding(dp(16), dp(10), dp(6), dp(10));
        GradientDrawable band = new GradientDrawable();
        band.setColor(color(R.color.cd_surface));
        band.setCornerRadii(new float[]{dp(16), dp(16), dp(16), dp(16), 0, 0, 0, 0});
        header.setBackground(band);
        ImageView logo = new ImageView(activity);
        logo.setImageResource(R.drawable.cd_mark);
        header.addView(logo, new LinearLayout.LayoutParams(dp(22), dp(22)));
        LinearLayout titles = new LinearLayout(activity);
        titles.setOrientation(LinearLayout.VERTICAL);
        titles.setPadding(dp(12), 0, 0, 0);
        TextView title = label("CloudDeck", 16, color(R.color.cd_text));
        title.setTypeface(title.getTypeface(), android.graphics.Typeface.BOLD);
        titles.addView(title);
        titles.addView(label(host.streamInfo(), 12, color(R.color.cd_muted)));
        header.addView(titles, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        ImageButton close = new ImageButton(activity);
        close.setImageResource(R.drawable.cd_ic_close);
        close.setBackgroundResource(R.drawable.cd_bg_ghost);
        close.setContentDescription(activity.getString(R.string.cd_menu_close));
        close.setOnClickListener(v -> dialog.dismiss());
        header.addView(close, new LinearLayout.LayoutParams(dp(48), dp(48)));
        root.addView(header);

        LinearLayout body = new LinearLayout(activity);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(12), dp(12), dp(12), dp(12));

        // ---- quick tiles
        List<View> tiles = new ArrayList<>();
        if (MicCapture.isActive()) {
            tiles.add(tile(R.drawable.cd_ic_mic, R.string.cd_card_mic, !MicCapture.isMuted(),
                    R.string.cd_on, R.string.cd_tile_muted, on -> { if (MicCapture.isMuted() == on) MicCapture.toggleMute(); }));
        }
        tiles.add(tile(R.drawable.cd_ic_stats, R.string.cd_tile_stats, host.isPerfOverlayVisible(),
                R.string.cd_on, R.string.cd_off, host::setPerfOverlayVisible));
        if (host.hasControllers()) {
            tiles.add(tile(R.drawable.cd_ic_mouse, R.string.cd_tile_controller, host.isControllerMouse(),
                    R.string.cd_tile_as_mouse, R.string.cd_tile_as_gamepad, host::setControllerMouse));
        }
        tiles.add(tile(R.drawable.cd_ic_devices, R.string.cd_tile_nintendo, host.isNintendoLayout(),
                R.string.cd_on, R.string.cd_off, host::setNintendoLayout));
        tiles.add(actionTile(R.drawable.cd_ic_keyboard, R.string.cd_tile_typing, R.string.cd_tile_typing_sub, () -> {
            dialog.dismiss();
            // Let the dialog's window go first, or the keyboard opens for it.
            button.postDelayed(host::toggleKeyboard, 200);
        }));
        tiles.add(actionTile(R.drawable.cd_ic_keyboard, R.string.cd_tile_full_keyboard, R.string.cd_tile_full_keyboard_sub, () -> {
            dialog.dismiss();
            host.toggleFullKeyboard();
        }));
        body.addView(grid(tiles, 3));

        // ---- on-screen controller
        final com.limelight.binding.input.virtual_controller.VirtualController osc = host.virtualController();
        if (osc != null) {
            LinearLayout card = card(R.string.cd_osc_card);
            card.addView(toggle(R.string.cd_osc_playstation,
                    OnScreenController.STYLE_PLAYSTATION.equals(OnScreenController.style(activity)), on -> {
                        OnScreenController.setStyle(activity, on ? OnScreenController.STYLE_PLAYSTATION : OnScreenController.STYLE_XBOX);
                        osc.refreshLayout();
                    }));
            LinearLayout row1 = row();
            row1.addView(action(R.string.cd_osc_edit_short, false, b -> {
                dialog.dismiss();
                android.widget.Toast.makeText(activity, R.string.cd_osc_edit_hint, android.widget.Toast.LENGTH_LONG).show();
                osc.startEditing();
            }), weight());
            row1.addView(action(R.string.cd_osc_save_short, false, b -> askLayoutName(osc)), weight());
            card.addView(row1);
            LinearLayout row2 = row();
            row2.addView(action(R.string.cd_osc_load_short, false, b -> pickLayout(osc, false)), weight());
            row2.addView(action(R.string.cd_osc_delete_short, false, b -> pickLayout(osc, true)), weight());
            card.addView(row2);
            body.addView(card);
        }

        // ---- session
        LinearLayout session = row();
        session.setPadding(0, dp(12), 0, 0);
        session.addView(action(R.string.cd_menu_disconnect, false, b -> {
            dialog.dismiss();
            host.disconnect();
        }), weight());
        if (CloudDeckActivity.launchedFromCloudDeck) {
            session.addView(action(R.string.cd_menu_shutdown_short, true, b -> {
                if (!confirmShutdown) {
                    confirmShutdown = true;
                    b.setText(R.string.cd_menu_shutdown_confirm);
                    GradientDrawable red = new GradientDrawable();
                    red.setColor(Color.parseColor("#C92A2A"));
                    red.setCornerRadius(dp(12));
                    b.setBackground(red);
                    return;
                }
                CloudDeckActivity.stopAfterStream = true;
                dialog.dismiss();
                host.disconnect();
            }), weight());
        }
        body.addView(session);
        body.addView(toggle(R.string.cd_menu_hide_button, buttonHidden(), on -> {
            prefs().edit().putBoolean(PREF_HIDE_BUTTON, on).apply();
            updateButton();
        }));
        body.addView(label(activity.getString(R.string.cd_menu_combo_hint), 12, color(R.color.cd_muted)));

        ScrollView scroll = new ScrollView(activity);
        scroll.addView(body);
        root.addView(scroll);
        return root;
    }

    private void askLayoutName(com.limelight.binding.input.virtual_controller.VirtualController osc) {
        final android.widget.EditText name = new android.widget.EditText(activity);
        name.setHint(R.string.cd_osc_name);
        new AlertDialog.Builder(activity)
                .setTitle(R.string.cd_osc_save)
                .setView(name)
                .setPositiveButton(android.R.string.ok, (d, w) -> {
                    String n = name.getText().toString().trim();
                    if (!n.isEmpty()) OnScreenController.saveLayout(osc, activity, n);
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void pickLayout(com.limelight.binding.input.virtual_controller.VirtualController osc, boolean delete) {
        final java.util.List<String> names = OnScreenController.layoutProfiles(activity);
        if (names.isEmpty()) {
            android.widget.Toast.makeText(activity, R.string.cd_osc_none, android.widget.Toast.LENGTH_SHORT).show();
            return;
        }
        new AlertDialog.Builder(activity)
                .setTitle(delete ? R.string.cd_osc_delete : R.string.cd_osc_load)
                .setItems(names.toArray(new String[0]), (d, which) -> {
                    if (delete) {
                        OnScreenController.deleteLayout(activity, names.get(which));
                    } else {
                        OnScreenController.loadLayout(osc, activity, names.get(which));
                        if (dialog != null) dialog.dismiss();
                    }
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    // ------------------------------------------------------------------ building blocks

    private interface OnToggle { void toggled(boolean on); }
    private interface OnAction { void clicked(Button b); }

    /** A toggle tile: icon, name, state line; tinted while on. */
    private View tile(int icon, int name, boolean on, int onText, int offText, OnToggle onToggle) {
        final LinearLayout t = tileBase(icon, name);
        final TextView status = (TextView) t.getChildAt(2);
        final boolean[] state = {on};
        Runnable paint = () -> {
            t.setActivated(state[0]);
            status.setText(state[0] ? onText : offText);
        };
        paint.run();
        t.setOnClickListener(v -> {
            state[0] = !state[0];
            onToggle.toggled(state[0]);
            paint.run();
        });
        return t;
    }

    /** A tile that does something once (e.g. opens a keyboard). */
    private View actionTile(int icon, int name, int sub, Runnable onClick) {
        LinearLayout t = tileBase(icon, name);
        ((TextView) t.getChildAt(2)).setText(sub);
        t.setOnClickListener(v -> onClick.run());
        return t;
    }

    private LinearLayout tileBase(int icon, int name) {
        LinearLayout t = new LinearLayout(activity);
        t.setOrientation(LinearLayout.VERTICAL);
        t.setBackgroundResource(R.drawable.cd_bg_tile);
        t.setPadding(dp(10), dp(10), dp(10), dp(10));
        t.setMinimumHeight(dp(84));
        t.setFocusable(true);
        t.setClickable(true);
        ImageView i = new ImageView(activity);
        i.setImageResource(icon);
        i.setDuplicateParentStateEnabled(true);
        t.addView(i, new LinearLayout.LayoutParams(dp(20), dp(20)));
        TextView n = label(activity.getString(name), 13, color(R.color.cd_text));
        n.setTypeface(n.getTypeface(), android.graphics.Typeface.BOLD);
        n.setPadding(0, dp(6), 0, 0);
        n.setSingleLine(true);
        n.setEllipsize(android.text.TextUtils.TruncateAt.END);
        t.addView(n);
        TextView s = label("", 12, 0);
        s.setTextColor(activity.getResources().getColorStateList(R.color.cd_tile_sub));
        s.setDuplicateParentStateEnabled(true);
        s.setSingleLine(true);
        s.setEllipsize(android.text.TextUtils.TruncateAt.END);
        t.addView(s);
        return t;
    }

    private View grid(List<View> cells, int columns) {
        LinearLayout g = new LinearLayout(activity);
        g.setOrientation(LinearLayout.VERTICAL);
        for (int i = 0; i < cells.size(); i += columns) {
            LinearLayout r = row();
            if (i > 0) r.setPadding(0, dp(8), 0, 0);
            for (int c = 0; c < columns; c++) {
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1);
                if (c > 0) lp.leftMargin = dp(8);
                if (i + c < cells.size()) r.addView(cells.get(i + c), lp);
                else r.addView(new View(activity), lp);
            }
            g.addView(r);
        }
        return g;
    }

    private LinearLayout card(int title) {
        LinearLayout card = new LinearLayout(activity);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackgroundResource(R.drawable.cd_bg_card);
        card.setPadding(dp(12), dp(10), dp(12), dp(12));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(12);
        card.setLayoutParams(lp);
        TextView t = label(activity.getString(title).toUpperCase(java.util.Locale.getDefault()), 12, color(R.color.cd_muted));
        t.setTypeface(t.getTypeface(), android.graphics.Typeface.BOLD);
        t.setLetterSpacing(0.05f);
        t.setPadding(dp(4), 0, 0, dp(4));
        card.addView(t);
        return card;
    }

    private LinearLayout row() {
        LinearLayout r = new LinearLayout(activity);
        r.setOrientation(LinearLayout.HORIZONTAL);
        return r;
    }

    private LinearLayout.LayoutParams weight() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(48), 1);
        lp.topMargin = dp(8);
        lp.leftMargin = dp(4);
        lp.rightMargin = dp(4);
        return lp;
    }

    private View toggle(int text, boolean checked, OnToggle onToggle) {
        Switch s = new Switch(activity);
        s.setText(text);
        s.setTextColor(color(R.color.cd_text));
        s.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        s.setChecked(checked);
        s.setMinHeight(dp(52));
        s.setPadding(dp(8), dp(6), dp(8), dp(6));
        s.setBackgroundResource(R.drawable.cd_bg_row);
        s.setOnCheckedChangeListener((v, on) -> onToggle.toggled(on));
        return s;
    }

    private Button action(int text, boolean danger, OnAction onAction) {
        Button b = new Button(activity);
        b.setText(text);
        b.setAllCaps(false);
        b.setStateListAnimator(null);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        b.setTextColor(danger ? color(R.color.cd_error) : color(R.color.cd_text));
        b.setBackgroundResource(R.drawable.cd_bg_secondary);
        b.setOnClickListener(v -> onAction.clicked(b));
        return b;
    }

    private TextView label(String text, int sp, int color) {
        TextView t = new TextView(activity);
        t.setText(text);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        return t;
    }

    @SuppressWarnings("deprecation")
    private int color(int id) { return activity.getResources().getColor(id); }

    private void updateButton() {
        button.setVisibility(buttonHidden() ? View.GONE : View.VISIBLE);
    }

    private boolean buttonHidden() {
        return prefs().getBoolean(PREF_HIDE_BUTTON, false);
    }

    private SharedPreferences prefs() {
        return PreferenceManager.getDefaultSharedPreferences(activity);
    }

    private int dp(int v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                activity.getResources().getDisplayMetrics()));
    }
}
