package net.tisma.printers

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import net.tisma.cloud.FormPart
import net.tisma.cloud.Http
import java.io.InputStream

/**
 * Klipper through Moonraker, from its public API documentation
 * (moonraker.readthedocs.io: external API, printer objects, webcams).
 * Moonraker may need no key on the local network; with one it goes in
 * `X-Api-Key`.
 */
class MoonrakerConnection(url: String, apiKey: String?, http: Http) : PrinterConnection {
    private val rest = Rest(url, if (apiKey.isNullOrBlank()) emptyMap() else mapOf("X-Api-Key" to apiKey), http)

    override fun test(): String {
        val r = result(rest.get("/server/info"))
        val version = r.str("moonraker_version")
        val klippy = r.str("klippy_state")
        return "Moonraker $version".trim() + if (klippy.isNotEmpty()) " (Klipper: $klippy)" else ""
    }

    override fun status(): PrinterStatus {
        val r = result(rest.get("/printer/objects/query?print_stats&virtual_sdcard&extruder&heater_bed&webhooks"))
        val s = r.o("status") ?: throw unexpected()
        val webhooks = s.o("webhooks")
        val stats = s.o("print_stats")
        val nozzle = temperature(s.o("extruder"), "temperature", "target")
        val bed = temperature(s.o("heater_bed"), "temperature", "target")
        // Klipper not ready (shutdown, error, startup): its message says why.
        val klippy = webhooks?.str("state").orEmpty()
        if (klippy.isNotEmpty() && klippy != "ready") {
            return PrinterStatus(if (klippy == "startup") PrinterState.BUSY else PrinterState.ERROR,
                nozzle = nozzle, bed = bed, message = webhooks?.str("state_message").orEmpty())
        }
        val state = when (stats?.str("state")) {
            "printing" -> PrinterState.PRINTING
            "paused" -> PrinterState.PAUSED
            "error" -> PrinterState.ERROR
            else -> PrinterState.READY // standby, complete, cancelled
        }
        val active = state == PrinterState.PRINTING || state == PrinterState.PAUSED
        val progress = s.o("virtual_sdcard")?.num("progress")
        val elapsed = stats?.num("print_duration")
        // Time left from the progress by file position: an estimate.
        val remaining = if (active && progress != null && progress > 0.001 && elapsed != null)
            ((elapsed / progress) - elapsed).toLong().coerceAtLeast(0) else null
        val info = stats?.o("info")
        return PrinterStatus(
            state = state,
            file = stats?.str("filename").orEmpty(),
            percent = if (active) progress?.times(100) else null,
            elapsedS = if (active) elapsed?.toLong() else null,
            remainingS = remaining,
            layer = info?.num("current_layer")?.toInt(),
            totalLayers = info?.num("total_layer")?.toInt(),
            nozzle = nozzle,
            bed = bed,
            message = stats?.str("message").orEmpty(),
            finished = when (stats?.str("state")) { "complete" -> true; "cancelled" -> false; else -> null },
        )
    }

    override fun pause() {
        rest.post("/printer/print/pause")
    }

    override fun resume() {
        rest.post("/printer/print/resume")
    }

    override fun cancel() {
        rest.post("/printer/print/cancel")
    }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> InputStream) {
        val parts = buildList {
            add(FormPart.text("root", "gcodes"))
            if (start) add(FormPart.text("print", "true"))
            add(FormPart("file", "application/octet-stream", name, length, open))
        }
        rest.upload("/server/files/upload", parts)
    }

    override fun setTemperature(heater: Heater, target: Double) =
        gcode(if (heater == Heater.NOZZLE) "M104 S${num(target)}" else "M140 S${num(target)}")

    override fun home(axes: Set<Axis>) = gcode("G28" + axes.sorted().joinToString("") { " " + it.name })

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) =
        gcode("G91\nG1 ${axis.name}${num(distance)} F${num(feedMmS * 60)}\nG90")

    override fun extrude(mm: Double, feedMmS: Double) = gcode("M83\nG1 E${num(mm)} F${num(feedMmS * 60)}")

    /** Runs G-code through Moonraker's script endpoint; Klipper's refusal comes back as the error. */
    private fun gcode(script: String) {
        rest.post("/printer/gcode/script?script=" + java.net.URLEncoder.encode(script, "UTF-8").replace("+", "%20"))
    }

    override fun camera(): Camera? {
        val r = result(rest.get("/server/webcams/list"))
        val cam = (r["webcams"] as? JsonArray)?.filterIsInstance<JsonObject>()?.firstOrNull { it.str("enabled") != "false" }
            ?: return null
        // printer_connections.md, «Cámara de la impresora»: each camera with its flip and rotation.
        return Camera(absolute(cam.str("stream_url")), absolute(cam.str("snapshot_url")),
            cam.str("flip_horizontal") == "true", cam.str("flip_vertical") == "true",
            (cam.str("rotation").toDoubleOrNull()?.toInt() ?: 0).mod(360))
    }

    /** A camera address may be relative to the printer's host (`/webcam/?action=stream`). */
    private fun absolute(u: String): String {
        if (u.isBlank() || u.contains("://")) return u
        val origin = rest.base.substringBefore("://") + "://" + rest.base.substringAfter("://").substringBefore('/')
            .substringBefore(':')
        return origin + (if (u.startsWith("/")) u else "/$u")
    }

    private fun result(e: kotlinx.serialization.json.JsonElement): JsonObject = e.obj()?.o("result") ?: throw unexpected()

    private fun unexpected() = PrinterException(PrinterException.Kind.UNEXPECTED, "Unexpected answer of Moonraker")

}
