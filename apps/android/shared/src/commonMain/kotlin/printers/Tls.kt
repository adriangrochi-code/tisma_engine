package net.tisma.printers

/**
 * TLS to a printer whose certificate no public authority signs (Bambu Lab, printer_connections.md, «Bambu Lab en red
 * local», rule 4): the first time, the certificate's SHA-256 fingerprint ([seenSha256]) and its name, the serial
 * number ([seenName]), are read for the person to confirm; after that only [expectedSha256] is accepted, and a
 * different certificate is a [PrinterException] of kind CERTIFICATE. Android uses its SSLSocket; iOS, NSStream with
 * the chain left unchecked and the fingerprint compared here.
 */
expect class PinnedTls(expectedSha256: String?) {
    var seenSha256: String?
        private set
    var seenName: String?
        private set

    /**
     * Connects to [host]:[port] with TLS and checks the certificate before anything is sent. [readTimeoutMs] limits a
     * read that waits (0: none, for MQTT, which may stay quiet for long).
     */
    fun open(host: String, port: Int, timeoutMs: Int = 10_000, readTimeoutMs: Int = 0): net.tisma.io.Link

    /** An FTPS data connection to [port], resuming the TLS session of the control connection on [sessionPort]. */
    fun openData(host: String, port: Int, sessionPort: Int, timeoutMs: Int = 10_000): net.tisma.io.Link
}
