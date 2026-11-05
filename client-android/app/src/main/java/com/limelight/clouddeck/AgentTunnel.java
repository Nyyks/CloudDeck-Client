package com.limelight.clouddeck;

import com.limelight.nvstream.http.LimelightCryptoProvider;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.SecureRandom;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;

import javax.net.ssl.KeyManager;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509KeyManager;
import javax.net.ssl.X509TrustManager;

/**
 * CloudDeck-Client: the tunnel to the host agent (docs/adr/0004-agent-tunnel-over-sunshine-https.md).
 * Same protocol as client-desktop/app/clouddeck/agenttunnel.cpp: TLS to Sunshine's HTTPS port
 * with the pairing identity, the host certificate pinned to the one learned at pairing, then
 * {@code GET /clouddeck/v1/<op>} with {@code Upgrade: clouddeck-agent/1}. 101 means the agent
 * took over the connection; anything else carries a readable reason in the body.
 */
final class AgentTunnel {
    static final class Connection {
        final SSLSocket socket;
        final InputStream in;
        final OutputStream out;

        Connection(SSLSocket socket, InputStream in, OutputStream out) {
            this.socket = socket;
            this.in = in;
            this.out = out;
        }

        /** One line from the agent, without the newline; null if the agent closed. */
        String readLine() throws IOException {
            ByteArrayOutputStream line = new ByteArrayOutputStream();
            int c;
            while ((c = in.read()) >= 0) {
                if (c == '\n') return line.toString("UTF-8").trim();
                if (line.size() > 65536) throw new IOException("line too long");
                line.write(c);
            }
            return null;
        }

        void close() {
            try { socket.close(); } catch (IOException ignored) {}
        }
    }

    static final class TunnelException extends IOException {
        TunnelException(String m) { super(m); }
    }

    private AgentTunnel() {}

    static Connection open(String host, int httpsPort, final X509Certificate serverCert,
                           final LimelightCryptoProvider crypto, String target, int timeoutMs) throws IOException {
        if (serverCert == null) throw new TunnelException("the CloudDeck is not paired");
        X509KeyManager km = new X509KeyManager() {
            public String chooseClientAlias(String[] keyTypes, Principal[] issuers, Socket socket) { return "Limelight-RSA"; }
            public String chooseServerAlias(String keyType, Principal[] issuers, Socket socket) { return null; }
            public X509Certificate[] getCertificateChain(String alias) { return new X509Certificate[]{crypto.getClientCertificate()}; }
            public String[] getClientAliases(String keyType, Principal[] issuers) { return null; }
            public PrivateKey getPrivateKey(String alias) { return crypto.getClientPrivateKey(); }
            public String[] getServerAliases(String keyType, Principal[] issuers) { return null; }
        };
        X509TrustManager tm = new X509TrustManager() {
            public void checkClientTrusted(X509Certificate[] chain, String authType) throws CertificateException {
                throw new CertificateException("not a server");
            }
            public void checkServerTrusted(X509Certificate[] chain, String authType) throws CertificateException {
                // Only the certificate learned at pairing, exactly like NvHTTP's pinned case.
                if (chain == null || chain.length == 0 || !chain[0].equals(serverCert)) {
                    throw new CertificateException("this is not your paired CloudDeck");
                }
            }
            public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
        };

        SSLSocket s;
        try {
            SSLContext sc = SSLContext.getInstance("TLS");
            sc.init(new KeyManager[]{km}, new TrustManager[]{tm}, new SecureRandom());
            s = (SSLSocket) sc.getSocketFactory().createSocket();
        } catch (java.security.GeneralSecurityException e) {
            throw new IOException(e);
        }
        try {
            s.connect(new InetSocketAddress(host, httpsPort), timeoutMs);
            s.setSoTimeout(timeoutMs);
            s.startHandshake();
            OutputStream out = s.getOutputStream();
            out.write(("GET /clouddeck/v1/" + target + " HTTP/1.1\r\nHost: " + host +
                    "\r\nUpgrade: clouddeck-agent/1\r\nConnection: Upgrade\r\n\r\n").getBytes(StandardCharsets.UTF_8));
            out.flush();

            InputStream in = new BufferedInputStream(s.getInputStream());
            Connection c = new Connection(s, in, out);
            String status = c.readLine();
            if (status == null) throw new TunnelException("the CloudDeck did not answer");
            int contentLength = -1;
            String h;
            while ((h = c.readLine()) != null && !h.isEmpty()) {
                if (h.toLowerCase(java.util.Locale.ROOT).startsWith("content-length:")) {
                    try { contentLength = Integer.parseInt(h.substring(15).trim()); } catch (NumberFormatException ignored) {}
                }
            }
            String[] parts = status.split(" ");
            int code = parts.length > 1 ? parseInt(parts[1]) : 0;
            if (code == 101) return c;
            if (code == 404) throw new TunnelException("the CloudDeck host update is not installed");
            String body = "";
            if (contentLength > 0 && contentLength < 65536) {
                byte[] b = new byte[contentLength];
                int n = 0, r;
                while (n < contentLength && (r = in.read(b, n, contentLength - n)) > 0) n += r;
                body = new String(b, 0, n, StandardCharsets.UTF_8).trim();
            }
            throw new TunnelException(body.isEmpty() ? "HTTP " + code : body);
        } catch (IOException e) {
            try { s.close(); } catch (IOException ignored) {}
            throw e;
        }
    }

    private static int parseInt(String s) {
        try { return Integer.parseInt(s); } catch (NumberFormatException e) { return 0; }
    }
}
