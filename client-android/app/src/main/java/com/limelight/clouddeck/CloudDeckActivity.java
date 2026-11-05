package com.limelight.clouddeck;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import com.limelight.LimeLog;
import com.limelight.PcView;
import com.limelight.R;
import com.limelight.binding.PlatformBinding;
import com.limelight.computers.ComputerManagerService;
import com.limelight.nvstream.http.ComputerDetails;
import com.limelight.nvstream.http.NvApp;
import com.limelight.nvstream.http.NvHTTP;
import com.limelight.nvstream.http.PairingManager;
import com.limelight.preferences.PreferenceConfiguration;
import com.limelight.preferences.StreamSettings;
import com.limelight.utils.ServerHelper;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.URL;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * CloudDeck-Client home screen: sign in, then one of two layouts the user picks in the
 * settings, "console" (pages and tiles; a sidebar on wide screens) or "launcher" (one big
 * button, settings in a side panel). Both share the settings sections cd_section_*.xml and
 * work with touch, D-pad, gamepads and TV remotes. See docs/adr/0013-home-layouts.md.
 * The connect flow mirrors client-desktop/app/clouddeck/cloudsession.cpp.
 */
public class CloudDeckActivity extends Activity {
    private static final long RUNNING_TIMEOUT_MS = 5 * 60_000;
    private static final long DNS_TIMEOUT_MS = 2 * 60_000;
    private static final long HOST_TIMEOUT_MS = 2 * 60_000;
    private static final int SUNSHINE_HTTP_PORT = 47989;

    public static final String PREF_UI_STYLE = "uiStyle";
    public static final String UI_CONSOLE = "console";
    public static final String UI_LAUNCHER = "launcher";
    // Moonlight's own preference keys (PreferenceConfiguration)
    private static final String PREF_FLIP_FACE_BUTTONS = "checkbox_flip_face_buttons";
    private static final String PREF_ONSCREEN_CONTROLLER = "checkbox_show_onscreen_controls";

    private final Handler ui = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private CloudDeckProvider provider;
    private SharedPreferences prefs;

    private CloudDeckApi.Machine machine = new CloudDeckApi.Machine();
    private boolean restoring, mfaRequired, streamLaunched, restartPending;
    // In-stream menu (docs/adr/0009-quick-menu.md): set while a stream started here runs,
    // and a one-shot "shut down my CloudDeck" request handled when it ends.
    public static volatile boolean launchedFromCloudDeck;
    public static volatile boolean stopAfterStream;
    private final AtomicBoolean connecting = new AtomicBoolean(false);
    private final AtomicBoolean cancel = new AtomicBoolean(false);

    private ComputerManagerService.ComputerManagerBinder managerBinder;
    private final ServiceConnection serviceConnection = new ServiceConnection() {
        public void onServiceConnected(ComponentName name, IBinder binder) {
            final ComputerManagerService.ComputerManagerBinder b = (ComputerManagerService.ComputerManagerBinder) binder;
            new Thread(() -> { b.waitForReady(); managerBinder = b; }).start();
        }
        public void onServiceDisconnected(ComponentName name) { managerBinder = null; }
    };

    // sign-in (fixed)
    private View restoringView, signInView;
    private FrameLayout homeContainer;
    private TextView signInError, noteView;
    private EditText emailField, passwordField, mfaField;
    private Button signInButton;

    // home (re-bound each time a layout is inflated)
    private View homeRoot, progressRow;
    private TextView statusView, hostView, statusLine, headlineView, progressView, errorView;
    private Button connectButton;
    private CompoundButton autoStop, micBox, nintendoBox, oscBox;
    private String uiStyle = UI_CONSOLE;
    private String page = "home";          // console page, or the launcher's settings tab
    private boolean drawerOpen;            // launcher settings panel
    private boolean focusPending;          // give Connect the D-pad focus once the home shows
    private boolean binding;               // suppresses listeners while switches are refreshed
    private Object backCallback;           // android.window.OnBackInvokedCallback on API 33+
    private boolean backCallbackRegistered;
    private static final int REQ_MIC = 7;

    private final Runnable pollTask = this::refreshStatus;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_clouddeck);
        provider = new CloudDeckProvider(this);
        prefs = getSharedPreferences("clouddeck", MODE_PRIVATE);

        restoringView = findViewById(R.id.cd_restoring);
        signInView = findViewById(R.id.cd_signin);
        homeContainer = findViewById(R.id.cd_home);
        signInError = findViewById(R.id.cd_signin_error);
        noteView = findViewById(R.id.cd_signin_note);
        emailField = findViewById(R.id.cd_email);
        passwordField = findViewById(R.id.cd_password);
        mfaField = findViewById(R.id.cd_mfa);
        signInButton = findViewById(R.id.cd_signin_button);

        noteView.setText(getString(R.string.cd_sign_in_note, SecretStore.backendName()));
        emailField.setText(provider.getEmail());
        signInButton.setOnClickListener(v -> onSignInClicked());
        TextView.OnEditorActionListener go = (v, action, ev) -> {
            if (action == EditorInfo.IME_ACTION_GO) { onSignInClicked(); return true; }
            return false;
        };
        passwordField.setOnEditorActionListener(go);
        mfaField.setOnEditorActionListener(go);
        findViewById(R.id.cd_signin_pcs).setOnClickListener(v -> startActivity(new Intent(this, PcView.class)));
        if (getResources().getConfiguration().screenWidthDp > 520) {
            findViewById(R.id.cd_signin_card).getLayoutParams().width = dp(440);
        }

        inflateHome();
        bindService(new Intent(this, ComputerManagerService.class), serviceConnection, BIND_AUTO_CREATE);

        restoring = true;
        render();
        worker.execute(() -> {
            String err = null;
            try {
                provider.restoreSession();
            } catch (Exception e) {
                err = e.getMessage();
            }
            final String fErr = err;
            ui.post(() -> {
                restoring = false;
                if (fErr != null) LimeLog.warning("CloudDeck: could not restore session: " + fErr);
                render();
                refreshStatus();
            });
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        renderToggles(); // the in-stream menu or the streaming settings may have changed them
        render();
        if (streamLaunched) {
            streamLaunched = false;
            launchedFromCloudDeck = false;
            if (stopAfterStream) {
                stopAfterStream = false;
                LimeLog.info("CloudDeck: shutdown requested from the in-stream menu");
                stopMachine();
                return;
            }
            if (prefs.getBoolean("autoStopOnSessionEnd", false) && CloudDeckApi.STATUS_RUNNING.equals(machine.status)) {
                LimeLog.info("CloudDeck: session ended, auto-stopping machine");
                stopMachine();
            }
        }
        refreshStatus();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_MIC && (grantResults.length == 0 || grantResults[0] != android.content.pm.PackageManager.PERMISSION_GRANTED)) {
            prefs.edit().putBoolean(MicCapture.PREF_ENABLED, false).apply();
            renderToggles();
            showError(getString(R.string.cd_mic_denied));
        }
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // The activity handles rotation and resizing itself (configChanges), so pick the
        // phone or wide layout again and come back to the same page.
        String keepPage = page;
        boolean keepDrawer = drawerOpen;
        View focused = getCurrentFocus();
        boolean hadFocus = focused != null && homeRoot != null && focused != connectButton;
        findViewById(R.id.cd_signin_card).getLayoutParams().width =
                newConfig.screenWidthDp > 520 ? dp(440) : ViewGroup.LayoutParams.MATCH_PARENT;
        inflateHome();
        focusPending = !hadFocus;
        render();
        if (keepDrawer) openSettings(keepPage, false);
        else if (!"home".equals(keepPage) && !isLauncher()) openSettings(keepPage, false);
    }

    @Override
    protected void onPause() {
        super.onPause();
        ui.removeCallbacks(pollTask);
    }

    @Override
    protected void onDestroy() {
        cancel.set(true);
        unbindService(serviceConnection);
        worker.shutdownNow();
        super.onDestroy();
    }

    // ------------------------------------------------------------------ home layouts

    private int dp(int v) { return Math.round(v * getResources().getDisplayMetrics().density); }

    @SuppressWarnings("deprecation")
    private int color(int id) { return getResources().getColor(id); }

    @SuppressWarnings("unchecked")
    private <T extends View> T v(int id) { return homeRoot == null ? null : (T) homeRoot.findViewById(id); }

    private void click(int id, Runnable r) {
        View view = v(id);
        if (view != null) view.setOnClickListener(x -> r.run());
    }

    private void setVisible(int id, boolean visible) {
        View view = v(id);
        if (view != null) view.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    private boolean isLauncher() { return UI_LAUNCHER.equals(uiStyle); }

    private SharedPreferences moonlightPrefs() { return PreferenceManager.getDefaultSharedPreferences(this); }

    /** (Re)builds the home screen in the layout chosen in the settings. */
    private void inflateHome() {
        uiStyle = UI_LAUNCHER.equals(prefs.getString(PREF_UI_STYLE, UI_CONSOLE)) ? UI_LAUNCHER : UI_CONSOLE;
        homeContainer.removeAllViews();
        homeRoot = getLayoutInflater().inflate(isLauncher() ? R.layout.cd_home_launcher : R.layout.cd_home_console,
                homeContainer, false);
        homeContainer.addView(homeRoot);
        findViewById(R.id.cd_root).setBackgroundColor(color(isLauncher() ? R.color.cd_bg_launcher : R.color.cd_bg));
        drawerOpen = false;
        focusPending = true;

        progressRow = v(R.id.cd_progress_row);
        progressView = v(R.id.cd_progress);
        errorView = v(R.id.cd_error);
        statusView = v(R.id.cd_status);
        hostView = v(R.id.cd_host);
        statusLine = v(R.id.cd_status_line);
        headlineView = v(R.id.cd_headline);
        connectButton = v(R.id.cd_connect);
        connectButton.setOnClickListener(x -> startAndConnect());
        click(R.id.cd_cancel, () -> cancel.set(true));

        // settings sections
        autoStop = v(R.id.cd_autostop);
        autoStop.setOnCheckedChangeListener((b, c) -> { if (!binding) prefs.edit().putBoolean("autoStopOnSessionEnd", c).apply(); });
        micBox = v(R.id.cd_mic);
        micBox.setOnCheckedChangeListener((b, c) -> {
            if (binding) return;
            prefs.edit().putBoolean(MicCapture.PREF_ENABLED, c).apply();
            if (c && !MicCapture.hasPermission(this) && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                requestPermissions(new String[]{android.Manifest.permission.RECORD_AUDIO}, REQ_MIC);
            }
            renderToggles();
        });
        nintendoBox = v(R.id.cd_nintendo);
        nintendoBox.setOnCheckedChangeListener((b, c) -> {
            if (binding) return;
            moonlightPrefs().edit().putBoolean(PREF_FLIP_FACE_BUTTONS, c).apply();
            renderToggles();
        });
        oscBox = v(R.id.cd_osc);
        oscBox.setOnCheckedChangeListener((b, c) -> {
            if (binding) return;
            moonlightPrefs().edit().putBoolean(PREF_ONSCREEN_CONTROLLER, c).apply();
            renderToggles();
        });
        click(R.id.cd_osc_xbox, () -> { OnScreenController.setStyle(this, OnScreenController.STYLE_XBOX); renderToggles(); });
        click(R.id.cd_osc_ps, () -> { OnScreenController.setStyle(this, OnScreenController.STYLE_PLAYSTATION); renderToggles(); });
        click(R.id.cd_controllers, () -> startActivity(new Intent(this, ControllerProfilesActivity.class)));
        click(R.id.cd_stream_settings, this::openStreamSettings);
        click(R.id.cd_style_console, () -> setUiStyle(UI_CONSOLE));
        click(R.id.cd_style_launcher, () -> setUiStyle(UI_LAUNCHER));
        click(R.id.cd_signout, this::signOut);
        click(R.id.cd_side_signout, this::signOut);
        click(R.id.cd_manage_pcs, this::managePcs);
        click(R.id.cd_link_pcs, this::managePcs);

        // quick actions on the home
        click(R.id.cd_chip_mic, () -> micBox.toggle());
        click(R.id.cd_tile_mic, () -> micBox.toggle());
        click(R.id.cd_chip_osc, () -> oscBox.toggle());
        click(R.id.cd_chip_nintendo, () -> nintendoBox.toggle());
        click(R.id.cd_tile_stream, this::openStreamSettings);
        click(R.id.cd_tile_power, this::powerAction);
        click(R.id.cd_link_power, this::powerAction);
        click(R.id.cd_tile_stop, this::confirmStop);
        click(R.id.cd_link_stop, this::confirmStop);

        // console pages / launcher tabs
        click(R.id.cd_nav_home, () -> showPage("home"));
        click(R.id.cd_nav_stream, () -> showPage("stream"));
        click(R.id.cd_nav_devices, () -> showPage("devices"));
        click(R.id.cd_nav_account, () -> showPage("account"));
        click(R.id.cd_tab_stream, () -> showPage("stream"));
        click(R.id.cd_tab_devices, () -> showPage("devices"));
        click(R.id.cd_tab_account, () -> showPage("account"));
        click(R.id.cd_settings, () -> openSettings("stream", false));
        click(R.id.cd_drawer_close, this::closeDrawer);
        click(R.id.cd_scrim, this::closeDrawer);

        if (isLauncher()) {
            int w = getResources().getConfiguration().screenWidthDp;
            if (w < 600) v(R.id.cd_drawer).getLayoutParams().width = ViewGroup.LayoutParams.MATCH_PARENT;
            if (w < 480) setVisible(R.id.cd_chip_nintendo, false);
        }
        showPage(isLauncher() ? "stream" : "home");
        renderToggles();
        updateBackCallback();
    }

    private void showPage(String p) {
        page = p;
        setVisible(R.id.cd_page_home, "home".equals(p));
        setVisible(R.id.cd_page_stream, "stream".equals(p));
        setVisible(R.id.cd_page_devices, "devices".equals(p));
        setVisible(R.id.cd_page_account, "account".equals(p));
        int[][] marks = {
            {R.id.cd_nav_home, "home".equals(p) ? 1 : 0},
            {R.id.cd_nav_stream, "stream".equals(p) ? 1 : 0}, {R.id.cd_tab_stream, "stream".equals(p) ? 1 : 0},
            {R.id.cd_nav_devices, "devices".equals(p) ? 1 : 0}, {R.id.cd_tab_devices, "devices".equals(p) ? 1 : 0},
            {R.id.cd_nav_account, "account".equals(p) ? 1 : 0}, {R.id.cd_tab_account, "account".equals(p) ? 1 : 0},
        };
        for (int[] m : marks) {
            View b = v(m[0]);
            if (b != null) b.setSelected(m[1] == 1);
        }
        TextView title = v(R.id.cd_page_title);
        if (title != null) {
            title.setVisibility("home".equals(p) ? View.GONE : View.VISIBLE);
            title.setText("stream".equals(p) ? R.string.cd_nav_stream : "devices".equals(p) ? R.string.cd_nav_devices : R.string.cd_nav_account);
        }
        ScrollView content = v(R.id.cd_content);
        if (content != null) content.scrollTo(0, 0);
        updateBackCallback();
    }

    private int navIdFor(String p) {
        if (isLauncher()) {
            return "devices".equals(p) ? R.id.cd_tab_devices : "account".equals(p) ? R.id.cd_tab_account : R.id.cd_tab_stream;
        }
        return "stream".equals(p) ? R.id.cd_nav_stream : "devices".equals(p) ? R.id.cd_nav_devices
                : "account".equals(p) ? R.id.cd_nav_account : R.id.cd_nav_home;
    }

    /** Shows a settings section: a console page, or the launcher's panel on that tab. */
    private void openSettings(String tab, boolean focusStyle) {
        if (isLauncher()) openDrawer();
        showPage(tab);
        View target = focusStyle ? v(isLauncher() ? R.id.cd_style_launcher : R.id.cd_style_console)
                                 : v(navIdFor(tab));
        if (target != null) target.requestFocus();
    }

    private void openDrawer() {
        View drawer = v(R.id.cd_drawer);
        if (drawer == null || drawerOpen) return;
        drawerOpen = true;
        ((ViewGroup) v(R.id.cd_launcher_main)).setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        setVisible(R.id.cd_scrim, true);
        drawer.setVisibility(View.VISIBLE);
        drawer.setTranslationX(drawer.getWidth() > 0 ? drawer.getWidth() : dp(400));
        drawer.animate().translationX(0).setDuration(180).start();
        updateBackCallback();
    }

    private void closeDrawer() {
        View drawer = v(R.id.cd_drawer);
        if (drawer == null || !drawerOpen) return;
        drawerOpen = false;
        ((ViewGroup) v(R.id.cd_launcher_main)).setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
        setVisible(R.id.cd_scrim, false);
        drawer.animate().translationX(drawer.getWidth()).setDuration(160)
                .withEndAction(() -> { if (!drawerOpen) drawer.setVisibility(View.GONE); }).start();
        View settings = v(R.id.cd_settings);
        if (settings != null) settings.requestFocus();
        updateBackCallback();
    }

    private void setUiStyle(String style) {
        if (style.equals(uiStyle)) return;
        prefs.edit().putString(PREF_UI_STYLE, style).apply();
        inflateHome();
        focusPending = false;
        render();
        // Take the user straight back to the switch they used, now in the other layout.
        ui.post(() -> openSettings("account", true));
    }

    /** Back (also gamepad B and the TV remote): close the panel, or go back to Home first. */
    private boolean handleBack() {
        if (drawerOpen) { closeDrawer(); return true; }
        if (!isLauncher() && !"home".equals(page) && provider.isSignedIn()) {
            showPage("home");
            View nav = v(R.id.cd_nav_home);
            if (nav != null) nav.requestFocus();
            return true;
        }
        return false;
    }

    private boolean wantsBack() {
        return drawerOpen || (!isLauncher() && !"home".equals(page));
    }

    private void updateBackCallback() {
        if (Build.VERSION.SDK_INT < 33) return;
        android.window.OnBackInvokedDispatcher d = getOnBackInvokedDispatcher();
        if (backCallback == null) backCallback = (android.window.OnBackInvokedCallback) this::handleBack;
        boolean want = wantsBack();
        if (want && !backCallbackRegistered) {
            d.registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                    (android.window.OnBackInvokedCallback) backCallback);
            backCallbackRegistered = true;
        } else if (!want && backCallbackRegistered) {
            d.unregisterOnBackInvokedCallback((android.window.OnBackInvokedCallback) backCallback);
            backCallbackRegistered = false;
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        if (!handleBack()) super.onBackPressed();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Menu / gamepad Start open the settings (and close the launcher's panel again).
        if ((keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_BUTTON_START) && provider.isSignedIn()) {
            if (drawerOpen) closeDrawer();
            else openSettings("stream", false);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    private void openStreamSettings() { startActivity(new Intent(this, StreamSettings.class)); }

    private void managePcs() { startActivity(new Intent(this, PcView.class)); }

    private void powerAction() {
        if (CloudDeckApi.STATUS_OFF.equals(machine.status)) startMachine();
        else confirm(R.string.cd_confirm_restart, () -> { restartPending = true; stopMachine(); });
    }

    private void confirmStop() { confirm(R.string.cd_confirm_stop, this::stopMachine); }

    private void signOut() {
        cancel.set(true);
        if (drawerOpen) closeDrawer();
        worker.execute(() -> { provider.signOut(); ui.post(() -> { machine = new CloudDeckApi.Machine(); render(); }); });
    }

    // ------------------------------------------------------------------ UI

    private void confirm(int msg, Runnable onYes) {
        new AlertDialog.Builder(this).setMessage(msg)
                .setPositiveButton(android.R.string.yes, (d, w) -> onYes.run())
                .setNegativeButton(android.R.string.no, null).show();
    }

    private void showError(String e) {
        boolean show = e != null && !e.isEmpty();
        for (TextView t : new TextView[]{errorView, signInError}) {
            if (t == null) continue;
            t.setText(show ? e : "");
            t.setVisibility(show ? View.VISIBLE : View.GONE);
        }
    }

    private void setProgress(String p) {
        ui.post(() -> {
            if (progressRow == null) return;
            progressView.setText(p);
            progressRow.setVisibility(p == null ? View.GONE : View.VISIBLE);
        });
    }

    private String statusLabel(String s) {
        switch (s) {
            case CloudDeckApi.STATUS_RUNNING: return getString(R.string.cd_status_running);
            case CloudDeckApi.STATUS_OFF: return getString(R.string.cd_status_off);
            case CloudDeckApi.STATUS_STARTING: return getString(R.string.cd_status_starting);
            case CloudDeckApi.STATUS_STOPPING: return getString(R.string.cd_status_stopping);
            case "": return getString(R.string.cd_status_unknown);
            default: return s;
        }
    }

    // Status colours, same as the desktop CdTheme.js: dot, soft background, ink.
    private static int[] statusColors(String s) {
        if (CloudDeckApi.STATUS_RUNNING.equals(s)) return new int[]{0xff3ddc84, 0xff12291c, 0xff6fe3a1};
        if (CloudDeckApi.isTransitional(s)) return new int[]{0xfffab005, 0xff2e2610, 0xffffd43b};
        if ("error".equals(s) || "failed".equals(s)) return new int[]{0xfffa5252, 0xff2f1517, 0xffff8787};
        return new int[]{0xff8a93a3, 0xff1f2329, 0xffc9cfda};
    }

    private String streamInfo() {
        PreferenceConfiguration c = PreferenceConfiguration.readPreferences(this);
        return getString(R.string.cd_stream_info, c.width, c.height, c.fps,
                String.format(Locale.getDefault(), "%.1f", c.bitrate / 1000.0).replaceAll("[.,]0$", ""));
    }

    private static void setEnabledDim(View view, boolean enabled) {
        if (view == null) return;
        view.setEnabled(enabled);
        view.setAlpha(enabled ? 1f : 0.45f);
    }

    private void render() {
        boolean signedIn = provider.isSignedIn();
        boolean busy = connecting.get();
        restoringView.setVisibility(restoring ? View.VISIBLE : View.GONE);
        signInView.setVisibility(!restoring && !signedIn ? View.VISIBLE : View.GONE);
        homeContainer.setVisibility(signedIn ? View.VISIBLE : View.GONE);
        mfaField.setVisibility(mfaRequired ? View.VISIBLE : View.GONE);
        emailField.setVisibility(mfaRequired ? View.GONE : View.VISIBLE);
        passwordField.setVisibility(mfaRequired ? View.GONE : View.VISIBLE);
        signInButton.setText(mfaRequired ? R.string.cd_verify : R.string.cd_sign_in);
        if (!signedIn) focusPending = true;

        String s = machine.status;
        boolean running = CloudDeckApi.STATUS_RUNNING.equals(s);
        boolean off = CloudDeckApi.STATUS_OFF.equals(s);
        int[] sc = statusColors(s);
        if (statusView != null) {
            statusView.setText("●  " + statusLabel(s));
            GradientDrawable pill = new GradientDrawable();
            pill.setCornerRadius(dp(17));
            pill.setColor(sc[1]);
            statusView.setBackground(pill);
            statusView.setTextColor(sc[2]);
        }
        String host = provider.hostAddress(machine);
        if (hostView != null) hostView.setText(machine.region.isEmpty() ? host : machine.region + "  ·  " + host);
        if (statusLine != null) {
            String line = off ? getString(R.string.cd_status_line_off)
                    : statusLabel(s) + (running && !machine.region.isEmpty() ? "  ·  " + machine.region : "");
            SpannableStringBuilder b = new SpannableStringBuilder("●  ").append(line);
            b.setSpan(new ForegroundColorSpan(sc[0]), 0, 1, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            statusLine.setText(b);
        }
        if (headlineView != null) {
            headlineView.setText(running ? R.string.cd_headline_running : off ? R.string.cd_headline_off
                    : CloudDeckApi.STATUS_STARTING.equals(s) ? R.string.cd_headline_starting
                    : CloudDeckApi.STATUS_STOPPING.equals(s) ? R.string.cd_headline_stopping : R.string.cd_headline_unknown);
        }
        connectButton.setText(running ? getString(R.string.cd_connect)
                : CloudDeckApi.STATUS_STARTING.equals(s) ? getString(R.string.cd_starting_ellipsis)
                : CloudDeckApi.STATUS_STOPPING.equals(s) ? getString(R.string.cd_stopping_ellipsis)
                : getString(R.string.cd_start_and_connect));
        setEnabledDim(connectButton, !busy && !CloudDeckApi.STATUS_STOPPING.equals(s));

        String info = streamInfo();
        TextView si = v(R.id.cd_stream_info);
        if (si != null) si.setText(info);
        TextView summary = v(R.id.cd_stream_summary);
        if (summary != null) summary.setText(info);

        TextView powerTitle = v(R.id.cd_tile_power_title);
        if (powerTitle != null) powerTitle.setText(off ? R.string.cd_start : R.string.cd_restart);
        TextView powerSub = v(R.id.cd_tile_power_sub);
        if (powerSub != null) powerSub.setText(off ? R.string.cd_tile_start_sub : R.string.cd_tile_restart_sub);
        Button powerLink = v(R.id.cd_link_power);
        if (powerLink != null) powerLink.setText(off ? R.string.cd_start : R.string.cd_restart);
        boolean canPower = !busy && (off || running);
        setEnabledDim(v(R.id.cd_tile_power), canPower);
        setEnabledDim(v(R.id.cd_link_power), canPower);
        setEnabledDim(v(R.id.cd_tile_stop), !busy && running);
        setEnabledDim(v(R.id.cd_link_stop), !busy && running);

        String email = provider.getEmail();
        for (int id : new int[]{R.id.cd_side_email, R.id.cd_top_email, R.id.cd_account_email}) {
            TextView t = v(id);
            if (t != null) t.setText(email);
        }

        // D-pad, gamepad and TV remote users start on Connect (no visible effect in touch mode).
        if (signedIn && focusPending && !restoring) {
            focusPending = false;
            connectButton.requestFocus();
        }
    }

    /** Puts the switches, chips and segments in line with the stored settings. */
    private void renderToggles() {
        if (homeRoot == null) return;
        SharedPreferences mp = moonlightPrefs();
        boolean mic = MicCapture.isEnabled(this);
        boolean nintendo = mp.getBoolean(PREF_FLIP_FACE_BUTTONS, false);
        boolean osc = mp.getBoolean(PREF_ONSCREEN_CONTROLLER, false);
        boolean ps = OnScreenController.STYLE_PLAYSTATION.equals(OnScreenController.style(this));
        binding = true;
        autoStop.setChecked(prefs.getBoolean("autoStopOnSessionEnd", false));
        micBox.setChecked(mic);
        nintendoBox.setChecked(nintendo);
        oscBox.setChecked(osc);
        binding = false;

        Button chipMic = v(R.id.cd_chip_mic);
        if (chipMic != null) { chipMic.setActivated(mic); chipMic.setText(mic ? R.string.cd_chip_mic_on : R.string.cd_chip_mic_off); }
        View chipOsc = v(R.id.cd_chip_osc);
        if (chipOsc != null) chipOsc.setActivated(osc);
        View chipNintendo = v(R.id.cd_chip_nintendo);
        if (chipNintendo != null) chipNintendo.setActivated(nintendo);
        View tileMic = v(R.id.cd_tile_mic);
        if (tileMic != null) tileMic.setActivated(mic);
        TextView tileMicSub = v(R.id.cd_tile_mic_sub);
        if (tileMicSub != null) tileMicSub.setText(mic ? R.string.cd_on : R.string.cd_off);
        View xbox = v(R.id.cd_osc_xbox), psb = v(R.id.cd_osc_ps);
        if (xbox != null) xbox.setSelected(!ps);
        if (psb != null) psb.setSelected(ps);
        View sc = v(R.id.cd_style_console), sl = v(R.id.cd_style_launcher);
        if (sc != null) sc.setSelected(!isLauncher());
        if (sl != null) sl.setSelected(isLauncher());
    }

    // ------------------------------------------------------------------ account

    private void onSignInClicked() {
        showError(null);
        signInButton.setEnabled(false);
        final boolean mfa = mfaRequired;
        final String email = emailField.getText().toString().trim();
        final String pw = passwordField.getText().toString();
        final String code = mfaField.getText().toString().trim();
        passwordField.setText("");
        mfaField.setText("");
        worker.execute(() -> {
            String err = null;
            boolean needMfa = false;
            try {
                if (mfa) {
                    provider.respondMfa(code);
                } else {
                    needMfa = provider.signIn(email, pw) == CloudDeckProvider.SignInResult.MFA_REQUIRED;
                }
            } catch (Exception e) {
                err = e.getMessage();
            }
            final String fErr = err;
            final boolean fNeedMfa = needMfa;
            ui.post(() -> {
                signInButton.setEnabled(true);
                mfaRequired = fNeedMfa;
                showError(fErr);
                render();
                if (provider.isSignedIn()) refreshStatus();
            });
        });
    }

    // ------------------------------------------------------------------ machine

    private void refreshStatus() {
        ui.removeCallbacks(pollTask);
        if (!provider.isSignedIn()) return;
        worker.execute(() -> {
            try {
                final CloudDeckApi.Machine m = provider.getMachine();
                ui.post(() -> onMachine(m));
            } catch (Exception e) {
                LimeLog.warning("CloudDeck: status refresh failed: " + e.getMessage());
                ui.post(() -> {
                    if (!provider.isSignedIn()) {
                        showError(getString(R.string.cd_err_session_expired));
                        render();
                    }
                    schedulePoll();
                });
            }
        });
    }

    private void onMachine(CloudDeckApi.Machine m) {
        machine = m;
        if (restartPending && CloudDeckApi.STATUS_OFF.equals(m.status)) {
            restartPending = false;
            startMachine();
        }
        render();
        schedulePoll();
    }

    private void schedulePoll() {
        boolean fast = CloudDeckApi.isTransitional(machine.status) || restartPending || connecting.get();
        ui.postDelayed(pollTask, fast ? 5000 : 30000);
    }

    private void startMachine() {
        showError(null);
        worker.execute(() -> {
            try {
                provider.startMachine(prefs.getString("sessionMode", ""));
                ui.post(() -> { machine.status = CloudDeckApi.STATUS_STARTING; render(); ui.postDelayed(pollTask, 3000); });
            } catch (Exception e) {
                ui.post(() -> showError(e.getMessage()));
            }
        });
    }

    private void stopMachine() {
        showError(null);
        worker.execute(() -> {
            try {
                provider.stopMachine();
                ui.post(() -> { machine.status = CloudDeckApi.STATUS_STOPPING; render(); ui.postDelayed(pollTask, 3000); });
            } catch (Exception e) {
                ui.post(() -> { restartPending = false; showError(e.getMessage()); });
            }
        });
    }

    // ------------------------------------------------------------------ start and connect

    private static class ConnectException extends Exception {
        ConnectException(String m) { super(m); }
    }

    private void checkCancelled() throws ConnectException {
        if (cancel.get()) throw new ConnectException(null);
    }

    private static void sleep(long ms) throws ConnectException {
        try { Thread.sleep(ms); } catch (InterruptedException e) { throw new ConnectException(null); }
    }

    private void startAndConnect() {
        if (!connecting.compareAndSet(false, true)) return;
        cancel.set(false);
        showError(null);
        render();
        final String appName = prefs.getString("appName", "CloudDeck");
        new Thread(() -> {
            String error = null;
            try {
                connectFlow(appName);
            } catch (ConnectException e) {
                error = e.getMessage();
            } catch (Exception e) {
                LimeLog.warning("CloudDeck: start-and-connect failed: " + e);
                error = e.getMessage();
            }
            final String fErr = error;
            ui.post(() -> {
                connecting.set(false);
                setProgress(null);
                if (fErr != null) showError(fErr);
                render();
                refreshStatus();
            });
        }, "CloudDeck-Connect").start();
    }

    private void connectFlow(String appName) throws Exception {
        // 1. Machine running
        long t0 = System.currentTimeMillis();
        CloudDeckApi.Machine m = provider.getMachine();
        if (CloudDeckApi.STATUS_OFF.equals(m.status)) {
            provider.startMachine(prefs.getString("sessionMode", ""));
        }
        while (!CloudDeckApi.STATUS_RUNNING.equals(m.status)) {
            checkCancelled();
            if ("error".equals(m.status) || "failed".equals(m.status)) {
                throw new ConnectException(getString(R.string.cd_err_start_failed));
            }
            long elapsed = System.currentTimeMillis() - t0;
            if (elapsed > RUNNING_TIMEOUT_MS) throw new ConnectException(getString(R.string.cd_err_start_timeout));
            setProgress(CloudDeckApi.STATUS_STOPPING.equals(m.status) ? getString(R.string.cd_progress_wait_stop)
                    : getString(R.string.cd_progress_starting, (int) (elapsed / 1000)));
            sleep(5000);
            m = provider.getMachine();
            if (CloudDeckApi.STATUS_OFF.equals(m.status)) {
                provider.startMachine(prefs.getString("sessionMode", ""));
            }
            final CloudDeckApi.Machine fm = m;
            ui.post(() -> { machine = fm; render(); });
        }

        // 2. DNS points at the machine's current public IP
        final String host = provider.hostAddress(m);
        setProgress(getString(R.string.cd_progress_dns));
        t0 = System.currentTimeMillis();
        while (true) {
            checkCancelled();
            if (m.publicIp.isEmpty()) {
                m = provider.getMachine();
            } else if (resolvesTo(host, m.publicIp)) {
                break;
            }
            if (System.currentTimeMillis() - t0 > DNS_TIMEOUT_MS) throw new ConnectException(getString(R.string.cd_err_dns, host));
            sleep(3000);
        }

        // 3. Sunshine answers serverinfo
        setProgress(getString(R.string.cd_progress_host));
        t0 = System.currentTimeMillis();
        while (!serverInfoReachable(host)) {
            checkCancelled();
            if (System.currentTimeMillis() - t0 > HOST_TIMEOUT_MS) throw new ConnectException(getString(R.string.cd_err_host_unreachable));
            sleep(3000);
        }

        // 4. Add host, auto-pair if needed
        setProgress(getString(R.string.cd_progress_connecting));
        while (managerBinder == null) { checkCancelled(); sleep(200); }
        ComputerDetails details = new ComputerDetails();
        details.manualAddress = new ComputerDetails.AddressTuple(host, SUNSHINE_HTTP_PORT);
        if (!managerBinder.addComputerBlocking(details)) {
            throw new ConnectException(getString(R.string.cd_err_add_failed, host));
        }
        ComputerDetails computer = managerBinder.getComputer(details.uuid);
        if (computer == null) computer = details;

        NvHTTP http = new NvHTTP(ServerHelper.getCurrentAddressFromComputer(computer), computer.httpsPort,
                managerBinder.getUniqueId(), computer.serverCert, PlatformBinding.getCryptoProvider(this));
        if (http.getPairState() != PairingManager.PairState.PAIRED) {
            setProgress(getString(R.string.cd_progress_pairing));
            final String pin = PairingManager.generatePinString();
            final AtomicBoolean pairingDone = new AtomicBoolean(false);
            final String[] pinError = new String[1];
            Thread pinThread = new Thread(() -> {
                // Let the pairing request reach Sunshine before the PIN does.
                try { Thread.sleep(1500); } catch (InterruptedException e) { return; }
                for (int attempt = 1; attempt <= 5 && !pairingDone.get(); attempt++) {
                    try {
                        provider.submitPairingPin(pin);
                        return;
                    } catch (Exception e) {
                        pinError[0] = e.getMessage();
                        LimeLog.warning("CloudDeck: PIN submission attempt " + attempt + " failed: " + e.getMessage());
                        try { Thread.sleep(2000); } catch (InterruptedException ie) { return; }
                    }
                }
            }, "CloudDeck-Pin");
            pinThread.start();
            PairingManager pm = http.getPairingManager();
            PairingManager.PairState state = pm.pair(http.getServerInfo(true), pin);
            pairingDone.set(true);
            pinThread.interrupt();
            if (state != PairingManager.PairState.PAIRED) {
                String why = pinError[0] != null ? getString(R.string.cd_err_pin_submit, pinError[0]) : state.toString();
                throw new ConnectException(getString(R.string.cd_err_pair_failed, why));
            }
            ComputerDetails stored = managerBinder.getComputer(computer.uuid);
            if (stored != null) stored.serverCert = pm.getPairedCert();
            managerBinder.invalidateStateForComputer(computer.uuid);
            http = new NvHTTP(ServerHelper.getCurrentAddressFromComputer(computer), computer.httpsPort,
                    managerBinder.getUniqueId(), pm.getPairedCert(), PlatformBinding.getCryptoProvider(this));
        }

        // 5. Launch the app
        setProgress(getString(R.string.cd_progress_launching, appName));
        NvApp app = null;
        List<NvApp> apps = http.getAppList();
        for (NvApp a : apps) {
            if (a.getAppName().equalsIgnoreCase(appName)) { app = a; break; }
        }
        if (app == null && !apps.isEmpty()) {
            // Host doesn't have the configured app (e.g. a non-CloudDeck app list): use its first app.
            app = apps.get(0);
            LimeLog.warning("CloudDeck: host has no app named " + appName + "; launching " + app.getAppName());
        }
        if (app == null) throw new ConnectException(getString(R.string.cd_err_no_app, appName));

        // Wait until the manager has a fresh, online view of the host for doStart().
        long t1 = System.currentTimeMillis();
        ComputerDetails ready;
        while (true) {
            checkCancelled();
            ready = managerBinder.getComputer(computer.uuid);
            if (ready != null && ready.state == ComputerDetails.State.ONLINE && ready.activeAddress != null) break;
            if (System.currentTimeMillis() - t1 > 30_000) throw new ConnectException(getString(R.string.cd_err_add_failed, host));
            sleep(500);
        }
        final NvApp fApp = app;
        final ComputerDetails fReady = ready;
        ui.post(() -> {
            streamLaunched = true;
            launchedFromCloudDeck = true;
            stopAfterStream = false;
            ServerHelper.doStart(CloudDeckActivity.this, fApp, fReady, managerBinder);
        });
    }

    private static boolean resolvesTo(String host, String ip) {
        try {
            for (InetAddress a : InetAddress.getAllByName(host)) {
                if (a.getHostAddress().equals(ip)) return true;
            }
        } catch (IOException ignored) {}
        return false;
    }

    private static boolean serverInfoReachable(String host) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL("http://" + host + ":" + SUNSHINE_HTTP_PORT + "/serverinfo").openConnection();
            c.setConnectTimeout(4000);
            c.setReadTimeout(4000);
            return c.getResponseCode() == 200;
        } catch (IOException e) {
            return false;
        } finally {
            if (c != null) c.disconnect();
        }
    }
}
