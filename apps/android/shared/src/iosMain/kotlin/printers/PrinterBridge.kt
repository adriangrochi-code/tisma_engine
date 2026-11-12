package net.tisma.printers

import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import net.tisma.cloud.PlatformHttp
import platform.Foundation.create

/**
 * A printer's state as the SwiftUI screen shows it. [percent] is -1 when not printing. [event] is what happened since
 * the last reading of the same printer (docs/MOBILE.md, «Avisos»): "finished", "stopped", "paused", "failed" or
 * empty; [eventDetail] is the line under the notice's title (the file, or the printer's own words).
 */
data class PrinterLine(
    val state: String,
    val file: String,
    val percent: Double,
    val nozzle: String,
    val bed: String,
    val event: String = "",
    val eventDetail: String = "",
) {
    companion object {
        /** The line for [after], with the event between [before] and it (the same rules as Android's watcher). */
        fun of(before: PrinterStatus?, after: PrinterStatus): PrinterLine {
            fun t(x: Temperature?) = x?.let { "${num(it.actual)} / ${num(it.target)} °C" } ?: ""
            val (event, detail) = when (val e = PrintEvents.between(before, after)) {
                null -> "" to ""
                is PrintEvent.Finished -> "finished" to e.file
                is PrintEvent.Stopped -> "stopped" to (if (e.state.isBlank()) e.file else "${e.file} · ${e.state}")
                is PrintEvent.Paused -> "paused" to e.message.ifBlank { e.file }
                is PrintEvent.Failed -> "failed" to e.message.ifBlank { e.file }
            }
            return PrinterLine(after.state.name.lowercase(), after.file, after.percent ?: -1.0, t(after.nozzle), t(after.bed),
                event, detail)
        }
    }
}

/** One image of the printer's camera (a JPEG) and how the printer's settings turn it. */
data class CameraShot(val jpeg: platform.Foundation.NSData, val flipHorizontal: Boolean, val flipVertical: Boolean, val rotation: Int)

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

    /** The last reading of each printer, to tell what happened between two (a print finished, paused…). */
    private val lastSeen = mutableMapOf<String, PrinterStatus>()

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
        val key = listOf(kind, address, user).joinToString("\u0000")
        // A failed reading throws before this: no reading is no event, and the last one stays for the next.
        val s = connection(kind, address, user, secret).status()
        val before = keptLock.locked { lastSeen[key].also { lastSeen[key] = s } }
        return PrinterLine.of(before, s)
    }

    @Throws(PrinterException::class)
    fun pause(kind: String, address: String, user: String, secret: String) = connection(kind, address, user, secret).pause()

    @Throws(PrinterException::class)
    fun resume(kind: String, address: String, user: String, secret: String) = connection(kind, address, user, secret).resume()

    @Throws(PrinterException::class)
    fun cancel(kind: String, address: String, user: String, secret: String) = connection(kind, address, user, secret).cancel()

    /**
     * Sends the G-code at [path] to the printer as [name] and, with [start], starts it (mobile_app.md, rule 14; MA7).
     * Read from the file in order, never whole in memory.
     */
    @OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)
    @Throws(PrinterException::class)
    fun upload(kind: String, address: String, user: String, secret: String, path: String, name: String, start: Boolean) {
        val length = platform.Foundation.NSFileManager.defaultManager.attributesOfItemAtPath(path, null)
            ?.get(platform.Foundation.NSFileSize)?.let { (it as? platform.Foundation.NSNumber)?.longLongValue } ?: -1L
        connection(kind, address, user, secret).upload(name, length, start) { net.tisma.io.FileInput(path) }
    }

    /** A camera that sends its own images (Bambu Lab A1 and P1), kept open while the camera is shown. */
    private var frames: Pair<String, CameraFrames>? = null

    /**
     * The next image of the printer's camera (printer_connections.md; mobile_app.md, rule 14; parity P18): its
     * snapshot address when it has one, else the images a Bambu Lab sends itself; null when it has no camera.
     */
    @OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)
    @Throws(PrinterException::class)
    fun cameraShot(kind: String, address: String, user: String, secret: String): CameraShot? {
        val c = connection(kind, address, user, secret)
        val cam = c.camera()
        val url = cam?.snapshotUrl?.takeIf { it.isNotBlank() }
        if (url != null) return CameraShot(Snapshot.fetch(url).toNSData(), cam.flipHorizontal, cam.flipVertical, cam.rotation)
        val key = listOf(kind, address, user, secret).joinToString("\u0000")
        val f = keptLock.locked {
            frames?.takeIf { it.first == key }?.second ?: c.cameraFrames()?.also { n ->
                frames?.second?.let { runCatching { it.close() } }
                frames = key to n
            }
        } ?: return null
        return CameraShot(f.next().toNSData(), false, false, 0)
    }

    /** Stops a camera that sends its own images (the screen no longer shows it). */
    fun stopCamera() = keptLock.locked { frames?.second?.let { runCatching { it.close() } }; frames = null }

    @OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)
    private fun ByteArray.toNSData(): platform.Foundation.NSData = if (isEmpty()) platform.Foundation.NSData() else
        usePinned { p -> platform.Foundation.NSData.create(bytes = p.addressOf(0), length = size.toULong()) }
}
