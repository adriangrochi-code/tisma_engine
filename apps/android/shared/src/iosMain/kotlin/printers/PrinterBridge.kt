package net.tisma.printers

import net.tisma.cloud.PlatformHttp

/** A printer's state as the SwiftUI screen shows it. [percent] is -1 when not printing. */
data class PrinterLine(
    val state: String,
    val file: String,
    val percent: Double,
    val nozzle: String,
    val bed: String,
)

/**
 * The network printers for the iPhone app (printer_connections.md), with the same clients as Android. A Bambu Lab's
 * certificate is pinned the first time «Test» reads it (rule 4: the serial and fingerprint are shown to the person);
 * Calls block and declare their errors for Swift.
 */
object PrinterBridge {
    /** The kinds the iPhone can use now, by their saved names. */
    val kinds: List<String> get() = PrinterKind.entries.map { it.wire }

    private val defaults get() = platform.Foundation.NSUserDefaults.standardUserDefaults
    private fun pinKey(address: String) = "printer.bambu.pin." + address.trim()

    /** The last connection, kept for the next call: Bambu Lab's MQTT link stays open between state readings. */
    private var kept: Pair<String, PrinterConnection>? = null
    private val keptLock = net.tisma.io.Lock()

    private fun connection(kind: String, address: String, user: String, secret: String): PrinterConnection = keptLock.locked {
        val key = listOf(kind, address, user, secret).joinToString("\u0000")
        kept?.takeIf { it.first == key }?.second ?: open(kind, address, user, secret).also { c ->
            kept?.second?.let { old -> runCatching { old.close() } }
            kept = key to c
        }
    }

    private fun open(kind: String, address: String, user: String, secret: String): PrinterConnection {
        val k = PrinterKind.of(kind) ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "Unknown printer kind: $kind")
        val url = PrinterConfig.normalizeUrl(address, k)
        return when (k) {
            PrinterKind.MOONRAKER -> MoonrakerConnection(url, secret.ifEmpty { null }, PlatformHttp)
            PrinterKind.OCTOPRINT -> OctoPrintConnection(url, secret, PlatformHttp)
            PrinterKind.PRUSALINK -> PrusaLinkConnection(url, user.ifBlank { "maker" }, secret, PlatformHttp)
            PrinterKind.DUET -> DuetConnection(url, secret.ifEmpty { "reprap" }, PlatformHttp)
            PrinterKind.BAMBU -> BambuConnection.of(url, secret, defaults.stringForKey(pinKey(address)))
        }
    }

    /** Checks the address and credentials; the program and its version, for the person. */
    @Throws(PrinterException::class)
    fun test(kind: String, address: String, user: String, secret: String): String {
        val c = open(kind, address, user, secret)
        try {
            val text = c.test()
            val b = c as? BambuConnection ?: return text
            // The first time, the certificate seen is kept: from then on only it is accepted.
            if (defaults.stringForKey(pinKey(address)) == null) b.seenFingerprint?.let { defaults.setObject(it, pinKey(address)) }
            return text + " · " + (b.seenFingerprint ?: "")
        } finally {
            c.close()
        }
    }

    /** Forgets a Bambu Lab's pinned certificate (the printer was replaced or reset). */
    fun forgetCertificate(address: String) = defaults.removeObjectForKey(pinKey(address))

    @Throws(PrinterException::class)
    fun status(kind: String, address: String, user: String, secret: String): PrinterLine {
        val s = connection(kind, address, user, secret).status()
        fun t(x: Temperature?) = x?.let { "${num(it.actual)} / ${num(it.target)} °C" } ?: ""
        return PrinterLine(s.state.name.lowercase(), s.file, s.percent ?: -1.0, t(s.nozzle), t(s.bed))
    }

    @Throws(PrinterException::class)
    fun pause(kind: String, address: String, user: String, secret: String) = connection(kind, address, user, secret).pause()

    @Throws(PrinterException::class)
    fun resume(kind: String, address: String, user: String, secret: String) = connection(kind, address, user, secret).resume()

    @Throws(PrinterException::class)
    fun cancel(kind: String, address: String, user: String, secret: String) = connection(kind, address, user, secret).cancel()
}
