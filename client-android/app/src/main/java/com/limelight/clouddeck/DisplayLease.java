package com.limelight.clouddeck;

import com.limelight.LimeLog;
import com.limelight.nvstream.http.LimelightCryptoProvider;

import java.io.IOException;
import java.net.SocketTimeoutException;
import java.security.cert.X509Certificate;

/**
 * CloudDeck-Client: switches the CloudDeck's desktop to the stream resolution for the length
 * of a stream, like client-desktop/app/clouddeck/displaylease.cpp (docs/adr/0007-multi-monitor.md).
 * Without it the VM keeps whatever mode it had and Sunshine scales the picture, so e.g. a
 * 1280x720 handheld would stream a shrunken 1920x1080 desktop. The agent restores the previous
 * mode as soon as the tunnel closes, also when the app is killed or the network drops.
 */
public final class DisplayLease {
    private static DisplayLease active;

    private final AgentTunnel.Connection conn;
    private final Thread keeper;
    private volatile boolean released;

    private DisplayLease(AgentTunnel.Connection conn) {
        this.conn = conn;
        keeper = new Thread(this::hold, "CloudDeck-DisplayLease");
        keeper.setDaemon(true);
    }

    /**
     * Blocks until the CloudDeck confirms the new mode (call off the UI thread). Replaces a lease
     * still held from an earlier stream. Returns null on success, otherwise a readable reason.
     */
    public static synchronized String acquire(String host, int httpsPort, X509Certificate serverCert,
                                              LimelightCryptoProvider crypto, int width, int height, int fps) {
        AgentTunnel.Connection c = null;
        try {
            c = AgentTunnel.open(host, httpsPort, serverCert, crypto,
                    "display/set?width=" + width + "&height=" + height + "&fps=" + fps, 15000);
            c.socket.setSoTimeout(40000);
            String line = c.readLine();
            if (line == null) throw new IOException("the CloudDeck did not confirm the display change");
            LimeLog.info("CloudDeck: display set for the stream: " + line);
            DisplayLease lease = new DisplayLease(c);
            // The old lease goes only now, so the host still restores its original mode at the end.
            if (active != null) active.close();
            active = lease;
            lease.keeper.start();
            return null;
        } catch (IOException e) {
            if (c != null) c.close();
            LimeLog.warning("CloudDeck: display change failed: " + e.getMessage());
            return e.getMessage() != null ? e.getMessage() : e.toString();
        }
    }

    /** Ends the lease of the last stream; the CloudDeck goes back to its previous mode. */
    public static synchronized void releaseActive() {
        if (active != null) {
            active.close();
            active = null;
        }
    }

    private void close() {
        released = true;
        conn.close();
    }

    private void hold() {
        long lastKeepalive = System.currentTimeMillis();
        try {
            conn.socket.setSoTimeout(1000);
            while (!released) {
                try {
                    if (conn.in.read() < 0) {
                        if (!released) LimeLog.warning("CloudDeck: display lease closed by the host");
                        return;
                    }
                } catch (SocketTimeoutException ignored) {
                    // nothing from the agent; check whether a keepalive is due
                }
                if (System.currentTimeMillis() - lastKeepalive > 30000) {
                    // A newline every 30 s keeps idle timeouts on the path away.
                    conn.out.write('\n');
                    conn.out.flush();
                    lastKeepalive = System.currentTimeMillis();
                }
            }
        } catch (IOException e) {
            if (!released) LimeLog.warning("CloudDeck: display lease lost: " + e.getMessage());
        } finally {
            conn.close();
            if (released) LimeLog.info("CloudDeck: display lease released");
        }
    }
}
