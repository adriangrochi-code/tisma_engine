package net.tisma.printers

import java.net.InetSocketAddress
import java.net.Socket
import java.security.MessageDigest
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.X509TrustManager

/**
 * TLS to a printer whose certificate no public authority signs (Bambu Lab,
 * printer_connections.md, «Bambu Lab en red local», rule 4): the first time,
 * the certificate's SHA-256 fingerprint and its name (the serial number) are
 * read for the person to confirm; after that only that fingerprint is
 * accepted. The name is not checked against the address: it is the serial,
 * not the IP. One context serves the MQTT and FTPS sockets, so the FTPS data
 * connection can resume the control connection's session.
 */
class PinnedTls(private val expectedSha256: String?) {
    @Volatile var seenSha256: String? = null
        private set
    @Volatile var seenName: String? = null
        private set

    private val trust = object : X509TrustManager {
        override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) =
            throw CertificateException("No client certificates")

        override fun checkServerTrusted(chain: Array<out X509Certificate>, authType: String?) {
            val leaf = chain.firstOrNull() ?: throw CertificateException("No certificate")
            val sha = fingerprint(leaf.encoded)
            seenSha256 = sha
            seenName = Regex("CN=([^,]+)").find(leaf.subjectX500Principal.name)?.groupValues?.get(1)
            if (expectedSha256 != null && !expectedSha256.equals(sha, ignoreCase = true)) {
                throw CertificateException("The printer's certificate changed")
            }
        }

        override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()
    }

    val context: SSLContext = SSLContext.getInstance("TLS").apply { init(null, arrayOf(trust), null) }

    fun socket(host: String, port: Int, timeoutMs: Int = 10_000): SSLSocket = wrap(
        Socket().apply { connect(InetSocketAddress(host, port), timeoutMs); soTimeout = 0 }, host, port)

    /**
     * Wraps a connected socket in TLS 1.2. [sessionPort] names the session to
     * resume: the FTPS data connection gives the control connection's port.
     */
    fun wrap(plain: Socket, host: String, sessionPort: Int): SSLSocket {
        val s = context.socketFactory.createSocket(plain, host, sessionPort, true) as SSLSocket
        s.enabledProtocols = s.supportedProtocols.filter { it == "TLSv1.2" }.ifEmpty { s.enabledProtocols.toList() }.toTypedArray()
        try {
            s.startHandshake()
        } catch (e: javax.net.ssl.SSLException) {
            if (e.cause is CertificateException || e.message?.contains("certificate changed") == true ||
                generateSequence(e as Throwable) { it.cause }.any { it is CertificateException }) {
                throw PrinterException(PrinterException.Kind.CERTIFICATE,
                    "The printer's certificate is not the one saved: it was not connected", e)
            }
            throw PrinterException(PrinterException.Kind.NETWORK, e.message ?: "TLS failed", e)
        }
        return s
    }

    companion object {
        fun fingerprint(der: ByteArray): String =
            MessageDigest.getInstance("SHA-256").digest(der).joinToString(":") { "%02X".format(it) }
    }
}
