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

/**
 * CloudDeck-Client: in-stream menu on Android (docs/adr/0009-quick-menu.md).
 * <p>
 * A small round button at the top centre (the CloudDeck mark) and the Back
 * gesture/button open a dark panel with: mute my microphone, performance stats,
 * keyboard, disconnect, shut down my CloudDeck.
 */
public final class StreamMenu {
    /** What the menu needs from the stream activity (implemented by Game). */
    public interface Host {
        boolean isPerfOverlayVisible();
        void setPerfOverlayVisible(boolean visible);
        boolean isNintendoLayout();
        void setNintendoLayout(boolean on);
        void toggleKeyboard();
        void disconnect();
        String streamInfo();
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
            bg.setColor(Color.parseColor("#1D222B"));
            bg.setCornerRadius(dp(14));
            dialog.getWindow().setBackgroundDrawable(bg);
        }
    }

    private View buildContent() {
        LinearLayout root = new LinearLayout(activity);
        root.setOrientation(LinearLayout.VERTICAL);

        // Header band in the CloudDeck colours.
        LinearLayout header = new LinearLayout(activity);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setPadding(dp(16), dp(14), dp(16), dp(14));
        GradientDrawable band = new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                new int[]{Color.parseColor("#3B5BDB"), Color.parseColor("#0B7285")});
        band.setCornerRadii(new float[]{dp(14), dp(14), dp(14), dp(14), 0, 0, 0, 0});
        header.setBackground(band);
        ImageView logo = new ImageView(activity);
        logo.setImageResource(R.drawable.cd_mark);
        header.addView(logo, new LinearLayout.LayoutParams(dp(24), dp(24)));
        LinearLayout titles = new LinearLayout(activity);
        titles.setOrientation(LinearLayout.VERTICAL);
        titles.setPadding(dp(12), 0, 0, 0);
        TextView title = label("CloudDeck", 17, Color.WHITE);
        title.setTypeface(title.getTypeface(), android.graphics.Typeface.BOLD);
        titles.addView(title);
        titles.addView(label(host.streamInfo(), 12, Color.parseColor("#DBE4FF")));
        header.addView(titles);
        root.addView(header);

        LinearLayout body = new LinearLayout(activity);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(16), dp(8), dp(16), dp(12));

        if (MicCapture.isActive()) {
            body.addView(toggle(R.string.cd_menu_mic_mute, MicCapture.isMuted(), on -> {
                if (MicCapture.isMuted() != on) MicCapture.toggleMute();
            }));
        }
        body.addView(toggle(R.string.cd_menu_stats, host.isPerfOverlayVisible(), host::setPerfOverlayVisible));
        body.addView(toggle(R.string.cd_menu_nintendo, host.isNintendoLayout(), host::setNintendoLayout));
        body.addView(toggle(R.string.cd_menu_hide_button, buttonHidden(), on -> {
            prefs().edit().putBoolean(PREF_HIDE_BUTTON, on).apply();
            updateButton();
        }));

        body.addView(action(R.string.cd_menu_keyboard, false, b -> {
            dialog.dismiss();
            // Let the dialog's window go first, or the keyboard opens for it.
            button.postDelayed(host::toggleKeyboard, 200);
        }));
        body.addView(action(R.string.cd_menu_disconnect, false, b -> {
            dialog.dismiss();
            host.disconnect();
        }));
        if (CloudDeckActivity.launchedFromCloudDeck) {
            body.addView(action(R.string.cd_menu_shutdown, true, b -> {
                if (!confirmShutdown) {
                    confirmShutdown = true;
                    b.setText(R.string.cd_menu_shutdown_confirm);
                    b.setBackgroundColor(Color.parseColor("#C92A2A"));
                    return;
                }
                CloudDeckActivity.stopAfterStream = true;
                dialog.dismiss();
                host.disconnect();
            }));
        }
        body.addView(action(R.string.cd_menu_close, false, b -> dialog.dismiss()));

        ScrollView scroll = new ScrollView(activity);
        scroll.addView(body);
        root.addView(scroll);
        return root;
    }

    private interface OnToggle { void toggled(boolean on); }
    private interface OnAction { void clicked(Button b); }

    private View toggle(int text, boolean checked, OnToggle onToggle) {
        Switch s = new Switch(activity);
        s.setText(text);
        s.setTextColor(Color.WHITE);
        s.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        s.setChecked(checked);
        s.setPadding(0, dp(10), 0, dp(10));
        s.setOnCheckedChangeListener((v, on) -> onToggle.toggled(on));
        return s;
    }

    private View action(int text, boolean danger, OnAction onAction) {
        Button b = new Button(activity);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(Color.WHITE);
        b.setBackgroundColor(Color.parseColor(danger ? "#3A2328" : "#2B3240"));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(44));
        lp.topMargin = dp(8);
        b.setLayoutParams(lp);
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
