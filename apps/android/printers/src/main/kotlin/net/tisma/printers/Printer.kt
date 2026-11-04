package net.tisma.printers

import java.io.InputStream

/**
 * A printer on the network, as the app sees it (docs/MOBILE.md, «Qué hace»;
 * printer_connections.md, «Programas que manejan impresoras»): test the
 * connection, read the state, pause, resume, cancel, upload a G-code and
 * start it, and the camera.
 *
 * Every call blocks; the app runs them off the main thread. A failure is a
 * [PrinterException]. This interface is where the engine's shared device
 * layer (src/device/, MOBILE.md) can take the place of these Kotlin clients.
 */
interface PrinterConnection {
    /** Checks the address and the credentials; returns the program and its version, for the person. */
    fun test(): String

    fun status(): PrinterStatus

    fun pause()

    fun resume()

    fun cancel()

    /** Uploads a G-code and, if [start], starts printing it. */
    fun upload(name: String, length: Long, start: Boolean, open: () -> InputStream)

    /** The camera's addresses, if the printer has one. */
    fun camera(): Camera?

    /** Whether the controls below work over this connection. */
    val hasControls: Boolean get() = true

    /** Ends a connection that stays open (Bambu Lab's MQTT); nothing for HTTP ones. */
    fun close() {}

    // ---- Controls (docs/MOBILE.md, «Controles»): temperatures any time; moving and extruding only when it is idle. ----

    /** Sets a heater's target in °C (0 turns it off). */
    fun setTemperature(heater: Heater, target: Double)

    /** Homes the given axes (all when empty). */
    fun home(axes: Set<Axis> = emptySet())

    /** Moves the head by a relative distance in mm, at [feedMmS] mm/s. */
    fun jog(axis: Axis, distance: Double, feedMmS: Double)

    /** Extrudes [mm] of filament (negative retracts), at [feedMmS] mm/s. The nozzle must be hot. */
    fun extrude(mm: Double, feedMmS: Double)
}

enum class Heater { NOZZLE, BED }

enum class Axis { X, Y, Z }

/** The kinds of connection the app speaks (mobile_app.md, rule 14). */
enum class PrinterKind(val wire: String, val defaultPort: Int) {
    MOONRAKER("moonraker", 7125),
    OCTOPRINT("octoprint", 80),
    PRUSALINK("prusalink", 80),
    DUET("duet", 80),
    BAMBU("bambu", 8883);

    companion object {
        fun of(wire: String) = entries.firstOrNull { it.wire == wire }
    }
}

/** What the printer is doing (MOBILE.md: disconnected, ready, printing, paused, error). */
enum class PrinterState { OFFLINE, READY, PRINTING, PAUSED, ERROR, BUSY }

data class Temperature(val actual: Double, val target: Double)

data class PrinterStatus(
    val state: PrinterState,
    /** The file being printed, or the last one. */
    val file: String = "",
    /** 0 to 100, or null when not printing. */
    val percent: Double? = null,
    val elapsedS: Long? = null,
    val remainingS: Long? = null,
    val layer: Int? = null,
    val totalLayers: Int? = null,
    val nozzle: Temperature? = null,
    val bed: Temperature? = null,
    val chamber: Temperature? = null,
    /** The printer's own words about an error or its state. */
    val message: String = "",
)

data class Camera(val streamUrl: String, val snapshotUrl: String)

/** A request to a printer failed. */
class PrinterException(val kind: Kind, val detail: String, cause: Throwable? = null) : Exception(detail, cause) {
    enum class Kind {
        /** No answer: the printer is off, or on another network. */
        NETWORK,
        /** The key or the password was refused (401, 403). */
        AUTH,
        /** The printer cannot do it now (409: not connected to its board, or not printing). */
        CONFLICT,
        /** This connection does not offer it (for example, PrusaLink has no moving by hand). */
        UNSUPPORTED,
        /** The printer's certificate is not the saved one (Bambu Lab, BL3). */
        CERTIFICATE,
        /** The answer is not what the API documents. */
        UNEXPECTED,
        /** Any other error of the printer's program. */
        SERVER,
    }
}

/** A saved printer: what the app keeps between runs. The key is kept apart, in the secret store. */
data class PrinterConfig(
    val id: String,
    val name: String,
    val kind: PrinterKind,
    /** The base address, e.g. `http://192.168.1.20:7125`. */
    val url: String,
    /** The user name, for connections that ask for one (PrusaLink: «maker» by default). */
    val user: String = "",
    /** Bambu Lab: the serial number (the certificate's name) and the confirmed certificate fingerprint. */
    val serial: String = "",
    val certSha256: String = "",
) {
    companion object {
        /** Adds `http://` when there is no scheme and the kind's port when there is none; drops trailing slashes. */
        fun normalizeUrl(address: String, kind: PrinterKind): String {
            var t = address.trim().trimEnd('/')
            if (t.isEmpty()) return t
            // Bambu Lab is not HTTP: its address is the host (and, for tests, other ports).
            if (kind == PrinterKind.BAMBU) return if (t.startsWith("bambu://")) t else "bambu://" + t.substringAfter("://")
            if (!t.startsWith("http://", true) && !t.startsWith("https://", true)) {
                t = "http://$t"
                val hostPart = t.removePrefix("http://").substringBefore('/')
                val hasPort = hostPart.substringAfterLast(']').contains(':')
                if (!hasPort && kind.defaultPort != 80) {
                    t = "http://$hostPart:${kind.defaultPort}" + t.removePrefix("http://$hostPart")
                }
            }
            return t
        }
    }
}

/** Opens the connection of a saved printer, with its key or password (null when it needs none). */
fun PrinterConfig.connect(secret: String?, http: net.tisma.cloud.Http = net.tisma.cloud.UrlConnectionHttp): PrinterConnection =
    when (kind) {
        PrinterKind.MOONRAKER -> MoonrakerConnection(url, secret, http)
        PrinterKind.OCTOPRINT -> OctoPrintConnection(url, secret.orEmpty(), http)
        PrinterKind.PRUSALINK -> PrusaLinkConnection(url, user.ifBlank { "maker" }, secret.orEmpty(), http)
        PrinterKind.DUET -> DuetConnection(url, secret.orEmpty().ifEmpty { "reprap" }, http)
        PrinterKind.BAMBU -> BambuConnection.of(url, secret.orEmpty(), certSha256.ifBlank { null })
    }
