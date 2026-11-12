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
 * The network printers for the iPhone app (printer_connections.md), with the same clients as Android. Bambu Lab
 * waits for its TLS on iOS (apps/ios/PLAN.md, step 2). Calls block and declare their errors for Swift.
 */
object PrinterBridge {
    /** The kinds the iPhone can use now, by their saved names. */
    val kinds: List<String> get() = PrinterKind.entries.filter { it != PrinterKind.BAMBU }.map { it.wire }

    private fun open(kind: String, address: String, user: String, secret: String): PrinterConnection {
        val k = PrinterKind.of(kind)?.takeIf { it != PrinterKind.BAMBU }
            ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "Not available on the iPhone yet: $kind")
        val url = PrinterConfig.normalizeUrl(address, k)
        return when (k) {
            PrinterKind.MOONRAKER -> MoonrakerConnection(url, secret.ifEmpty { null }, PlatformHttp)
            PrinterKind.OCTOPRINT -> OctoPrintConnection(url, secret, PlatformHttp)
            PrinterKind.PRUSALINK -> PrusaLinkConnection(url, user.ifBlank { "maker" }, secret, PlatformHttp)
            PrinterKind.DUET -> DuetConnection(url, secret.ifEmpty { "reprap" }, PlatformHttp)
            PrinterKind.BAMBU -> error("unreachable")
        }
    }

    /** Checks the address and credentials; the program and its version, for the person. */
    @Throws(PrinterException::class)
    fun test(kind: String, address: String, user: String, secret: String): String = open(kind, address, user, secret).test()

    @Throws(PrinterException::class)
    fun status(kind: String, address: String, user: String, secret: String): PrinterLine {
        val s = open(kind, address, user, secret).status()
        fun t(x: Temperature?) = x?.let { "${num(it.actual)} / ${num(it.target)} °C" } ?: ""
        return PrinterLine(s.state.name.lowercase(), s.file, s.percent ?: -1.0, t(s.nozzle), t(s.bed))
    }

    @Throws(PrinterException::class)
    fun pause(kind: String, address: String, user: String, secret: String) = open(kind, address, user, secret).pause()

    @Throws(PrinterException::class)
    fun resume(kind: String, address: String, user: String, secret: String) = open(kind, address, user, secret).resume()

    @Throws(PrinterException::class)
    fun cancel(kind: String, address: String, user: String, secret: String) = open(kind, address, user, secret).cancel()
}
