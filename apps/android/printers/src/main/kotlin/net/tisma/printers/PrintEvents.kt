package net.tisma.printers

/** What the person is told about a print (docs/MOBILE.md, «Avisos»). */
sealed interface PrintEvent {
    val file: String

    data class Finished(override val file: String) : PrintEvent
    /** Stopped before the end: cancelled at the printer, or by the app. */
    data class Stopped(override val file: String, val state: String = "") : PrintEvent
    /** Paused: by hand or by the printer itself (for example, out of filament); its message says why. */
    data class Paused(override val file: String, val message: String) : PrintEvent
    data class Failed(override val file: String, val message: String) : PrintEvent
}

object PrintEvents {
    /**
     * The event between two readings of the same printer, or null. A missing
     * reading (the printer did not answer) is no event: a print is not
     * «finished» because the Wi-Fi dropped.
     */
    fun between(before: PrinterStatus?, after: PrinterStatus?): PrintEvent? {
        if (before == null || after == null || after.state == PrinterState.OFFLINE) return null
        val was = before.state
        val now = after.state
        val active = was == PrinterState.PRINTING || was == PrinterState.PAUSED
        val file = after.file.ifBlank { before.file }
        return when {
            now == PrinterState.ERROR && was != PrinterState.ERROR && active -> PrintEvent.Failed(file, after.message)
            now == PrinterState.PAUSED && was == PrinterState.PRINTING -> PrintEvent.Paused(file, after.message)
            // A state the printer names but no specification documents (Bambu Lab, printer_connections.md, rule 5)
            // comes as BUSY with its name: leaving the print for it is an end like READY, judged by the progress.
            (now == PrinterState.READY || now == PrinterState.BUSY) && active ->
                if ((before.percent ?: 0.0) >= 99.0) PrintEvent.Finished(file) else PrintEvent.Stopped(file, after.message)
            else -> null
        }
    }

    fun isActive(s: PrinterStatus?) = s?.state == PrinterState.PRINTING || s?.state == PrinterState.PAUSED
}
