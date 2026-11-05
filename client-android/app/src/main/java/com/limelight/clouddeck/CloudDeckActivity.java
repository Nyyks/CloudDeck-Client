package com.limelight.clouddeck;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
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
import com.limelight.utils.ServerHelper;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.URL;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * CloudDeck-Client home screen: sign in, VM card and one-click "Start and connect".
 * Mirrors client-desktop/app/clouddeck/cloudsession.cpp.
 */
public class CloudDeckActivity extends Activity {
    private static final long RUNNING_TIMEOUT_MS = 5 * 60_000;
    private static final long DNS_TIMEOUT_MS = 2 * 60_000;
    private static final long HOST_TIMEOUT_MS = 2 * 60_000;
    private static final int SUNSHINE_HTTP_PORT = 47989;

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

    private View restoringView, signInView, cardView, progressRow;
    private TextView statusView, hostView, progressView, errorView, noteView;
    private EditText emailField, passwordField, mfaField;
    private Button signInButton, connectButton, startButton, stopButton, restartButton;
    private CheckBox autoStop, micBox;
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
        cardView = findViewById(R.id.cd_card);
        progressRow = findViewById(R.id.cd_progress_row);
        statusView = findViewById(R.id.cd_status);
        hostView = findViewById(R.id.cd_host);
        progressView = findViewById(R.id.cd_progress);
        errorView = findViewById(R.id.cd_error);
        noteView = findViewById(R.id.cd_signin_note);
        emailField = findViewById(R.id.cd_email);
        passwordField = findViewById(R.id.cd_password);
        mfaField = findViewById(R.id.cd_mfa);
        signInButton = findViewById(R.id.cd_signin_button);
        connectButton = findViewById(R.id.cd_connect);
        startButton = findViewById(R.id.cd_start);
        stopButton = findViewById(R.id.cd_stop);
        restartButton = findViewById(R.id.cd_restart);
        autoStop = findViewById(R.id.cd_autostop);

        noteView.setText(getString(R.string.cd_sign_in_note, SecretStore.backendName()));
        emailField.setText(provider.getEmail());
        autoStop.setChecked(prefs.getBoolean("autoStopOnSessionEnd", false));
        autoStop.setOnCheckedChangeListener((v, c) -> prefs.edit().putBoolean("autoStopOnSessionEnd", c).apply());

        micBox = findViewById(R.id.cd_mic);
        micBox.setChecked(MicCapture.isEnabled(this));
        micBox.setOnCheckedChangeListener((v, c) -> {
            prefs.edit().putBoolean(MicCapture.PREF_ENABLED, c).apply();
            if (c && !MicCapture.hasPermission(this) && android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
                requestPermissions(new String[]{android.Manifest.permission.RECORD_AUDIO}, REQ_MIC);
            }
        });

        signInButton.setOnClickListener(v -> onSignInClicked());
        connectButton.setOnClickListener(v -> startAndConnect());
        startButton.setOnClickListener(v -> startMachine());
        stopButton.setOnClickListener(v -> confirm(R.string.cd_confirm_stop, this::stopMachine));
        restartButton.setOnClickListener(v -> confirm(R.string.cd_confirm_restart, () -> { restartPending = true; stopMachine(); }));
        findViewById(R.id.cd_cancel).setOnClickListener(v -> cancel.set(true));
        findViewById(R.id.cd_manage_pcs).setOnClickListener(v -> startActivity(new Intent(this, PcView.class)));
        findViewById(R.id.cd_signout).setOnClickListener(v -> {
            cancel.set(true);
            worker.execute(() -> { provider.signOut(); ui.post(() -> { machine = new CloudDeckApi.Machine(); render(); }); });
        });

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
        if (streamLaunched) {
            streamLaunched = false;
            launchedFromCloudDeck = false;
            if (stopAfterStream) {
                stopAfterStream = false;
                LimeLog.info("CloudDeck: shutdown requested from the in-stream menu");
                stopMachine();
                return;
            }
            if (autoStop.isChecked() && CloudDeckApi.STATUS_RUNNING.equals(machine.status)) {
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
            micBox.setChecked(false);
            showError(getString(R.string.cd_mic_denied));
        }
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

    // ------------------------------------------------------------------ UI

    private void confirm(int msg, Runnable onYes) {
        new AlertDialog.Builder(this).setMessage(msg)
                .setPositiveButton(android.R.string.yes, (d, w) -> onYes.run())
                .setNegativeButton(android.R.string.no, null).show();
    }

    private void showError(String e) {
        errorView.setText(e == null ? "" : e);
        errorView.setVisibility(e == null || e.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void setProgress(String p) {
        ui.post(() -> {
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

    private void render() {
        boolean signedIn = provider.isSignedIn();
        boolean busy = connecting.get();
        restoringView.setVisibility(restoring ? View.VISIBLE : View.GONE);
        signInView.setVisibility(!restoring && !signedIn ? View.VISIBLE : View.GONE);
        cardView.setVisibility(signedIn ? View.VISIBLE : View.GONE);
        mfaField.setVisibility(mfaRequired ? View.VISIBLE : View.GONE);
        emailField.setVisibility(mfaRequired ? View.GONE : View.VISIBLE);
        passwordField.setVisibility(mfaRequired ? View.GONE : View.VISIBLE);
        signInButton.setText(mfaRequired ? R.string.cd_verify : R.string.cd_sign_in);

        String s = machine.status;
        statusView.setText(statusLabel(s));
        String host = provider.hostAddress(machine);
        hostView.setText(machine.region.isEmpty() ? host : host + "  ·  " + machine.region);
        connectButton.setText(CloudDeckApi.STATUS_RUNNING.equals(s) ? R.string.cd_connect : R.string.cd_start_and_connect);
        connectButton.setEnabled(!busy && !CloudDeckApi.STATUS_STOPPING.equals(s));
        startButton.setEnabled(!busy && CloudDeckApi.STATUS_OFF.equals(s));
        stopButton.setEnabled(!busy && CloudDeckApi.STATUS_RUNNING.equals(s));
        restartButton.setEnabled(!busy && CloudDeckApi.STATUS_RUNNING.equals(s));
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
