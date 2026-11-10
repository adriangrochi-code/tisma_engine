package net.tisma.printers

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import net.tisma.cloud.FormPart
import net.tisma.cloud.Http
import java.io.InputStream

/**
 * OctoPrint, from its REST API documentation (docs.octoprint.org: General
 * information, Version, Printer operations, Job operations, File operations,
 * Settings). The key goes in `X-Api-Key`.
 */
class OctoPrintConnection(url: String, apiKey: String, http: Http) : PrinterConnection {
    private val rest = Rest(url, mapOf("X-Api-Key" to apiKey), http)

    override fun test(): String {
        val v = rest.get("/api/version").obj() ?: throw unexpected()
        return v.str("text").ifBlank { "OctoPrint " + v.str("server") }
    }

    override fun status(): PrinterStatus {
        // 409 here means OctoPrint runs but is not connected to the printer's board.
        val printer = try {
            rest.get("/api/printer").obj() ?: throw unexpected()
        } catch (e: PrinterException) {
            if (e.kind != PrinterException.Kind.CONFLICT) throw e
            return PrinterStatus(PrinterState.OFFLINE, message = e.detail)
        }
        val job = rest.get("/api/job").obj() ?: throw unexpected()
        val flags = printer.o("state")?.o("flags")
        val temps = printer.o("temperature")
        val state = when {
            flags == null -> PrinterState.OFFLINE
            flags.flag("error") || flags.flag("closedOrError") && !flags.flag("operational") -> PrinterState.ERROR
            flags.flag("paused") || flags.flag("pausing") -> PrinterState.PAUSED
            flags.flag("printing") || flags.flag("cancelling") || flags.flag("resuming") -> PrinterState.PRINTING
            flags.flag("operational") -> PrinterState.READY
            else -> PrinterState.OFFLINE
        }
        val active = state == PrinterState.PRINTING || state == PrinterState.PAUSED
        val progress = job.o("progress")
        return PrinterStatus(
            state = state,
            file = job.o("job")?.o("file")?.str("name").orEmpty(),
            percent = if (active) progress?.num("completion") else null,
            elapsedS = if (active) progress?.num("printTime")?.toLong() else null,
            remainingS = if (active) progress?.num("printTimeLeft")?.toLong() else null,
            nozzle = temperature(temps?.o("tool0")),
            bed = temperature(temps?.o("bed")),
            chamber = temperature(temps?.o("chamber")),
            message = if (state == PrinterState.ERROR) job.str("error").ifBlank { printer.o("state")?.str("text").orEmpty() }
            else "",
        )
    }

    override fun pause() {
        rest.post("/api/job", command("pause") { put("action", "pause") })
    }

    override fun resume() {
        rest.post("/api/job", command("pause") { put("action", "resume") })
    }

    override fun cancel() {
        rest.post("/api/job", command("cancel"))
    }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> InputStream) {
        val parts = buildList {
            add(FormPart("file", "application/octet-stream", name, length, open))
            if (start) {
                add(FormPart.text("select", "true"))
                add(FormPart.text("print", "true"))
            }
        }
        rest.upload("/api/files/local", parts)
    }

    override fun setTemperature(heater: Heater, target: Double) {
        if (heater == Heater.NOZZLE) rest.post("/api/printer/tool", command("target") {
            put("targets", buildJsonObject { put("tool0", target) })
        }) else rest.post("/api/printer/bed", command("target") { put("target", target) })
    }

    override fun home(axes: Set<Axis>) {
        val list = (axes.ifEmpty { Axis.entries.toSet() }).sorted().map { it.name.lowercase() }
        rest.post("/api/printer/printhead", command("home") {
            put("axes", kotlinx.serialization.json.JsonArray(list.map { kotlinx.serialization.json.JsonPrimitive(it) }))
        })
    }

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) {
        rest.post("/api/printer/printhead", command("jog") {
            put(axis.name.lowercase(), distance)
            put("speed", feedMmS * 60)
        })
    }

    override fun extrude(mm: Double, feedMmS: Double) {
        rest.post("/api/printer/tool", command("extrude") {
            put("amount", mm)
            put("speed", feedMmS * 60)
        })
    }

    override fun camera(): Camera? {
        val webcam = rest.get("/api/settings").obj()?.o("webcam") ?: return null
        if (webcam.str("webcamEnabled") == "false") return null
        val stream = absolute(webcam.str("streamUrl"))
        val snapshot = absolute(webcam.str("snapshotUrl"))
        if (stream.isBlank() && snapshot.isBlank()) return null
        // OctoPrint's webcam settings: flipH, flipV and rotate90 (90° counter-clockwise, so 270° clockwise).
        return Camera(stream, snapshot, webcam.str("flipH") == "true", webcam.str("flipV") == "true",
            if (webcam.str("rotate90") == "true") 270 else 0)
    }

    private fun absolute(u: String): String {
        if (u.isBlank() || u.contains("://")) return u
        val origin = rest.base.substringBefore("://") + "://" + rest.base.substringAfter("://").substringBefore('/')
        return origin + (if (u.startsWith("/")) u else "/$u")
    }

    private fun command(name: String, extra: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}): JsonObject =
        buildJsonObject { put("command", name); extra() }

    private fun unexpected() = PrinterException(PrinterException.Kind.UNEXPECTED, "Unexpected answer of OctoPrint")
}
