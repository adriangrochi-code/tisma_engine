package net.tisma.printers

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import net.tisma.cloud.FormPart
import net.tisma.cloud.Http
import java.io.InputStream
import java.net.URLEncoder

/**
 * PrusaLink (Prusa printers' local interface), from its published OpenAPI
 * specification (printer_connections.md, «Fuentes»): HTTP Digest with the
 * user and password shown on the printer; `GET /api/version`,
 * `GET /api/v1/status`, `GET /api/v1/job`, pause and resume by
 * `PUT /api/v1/job/{id}/pause|resume`, stop by `DELETE /api/v1/job/{id}`,
 * and upload by `PUT /api/v1/files/{storage}/{name}` with `Print-After-Upload`.
 * The v1 API has no moving by hand nor camera stream.
 */
class PrusaLinkConnection(url: String, user: String, password: String, http: Http) : PrinterConnection {
    private val rest = Rest(url, emptyMap(), http, Digest(user, password))

    override val hasControls: Boolean get() = false

    override fun test(): String {
        val v = rest.get("/api/version").obj() ?: throw unexpected()
        return v.str("text").ifBlank { "PrusaLink " + v.str("server") } +
            v.str("printer").let { if (it.isBlank()) "" else " ($it)" }
    }

    override fun status(): PrinterStatus {
        val s = rest.get("/api/v1/status").obj() ?: throw unexpected()
        val p = s.o("printer") ?: throw unexpected()
        val word = p.str("state").uppercase()
        val state = when (word) {
            "PRINTING" -> PrinterState.PRINTING
            "PAUSED" -> PrinterState.PAUSED
            "BUSY" -> PrinterState.BUSY
            "ERROR", "ATTENTION" -> PrinterState.ERROR
            else -> PrinterState.READY // IDLE, READY, FINISHED, STOPPED
        }
        val active = state == PrinterState.PRINTING || state == PrinterState.PAUSED
        val job = s.o("job")
        val file = if (active) runCatching { rest.get("/api/v1/job").obj()?.o("file") }.getOrNull() else null
        return PrinterStatus(
            state = state,
            file = file?.str("display_name")?.ifBlank { file.str("name") }.orEmpty(),
            percent = if (active) job?.num("progress") else null,
            elapsedS = if (active) job?.num("time_printing")?.toLong() else null,
            remainingS = if (active) job?.num("time_remaining")?.toLong() else null,
            nozzle = p.num("temp_nozzle")?.let { Temperature(it, p.num("target_nozzle") ?: 0.0) },
            bed = p.num("temp_bed")?.let { Temperature(it, p.num("target_bed") ?: 0.0) },
            message = if (state == PrinterState.ERROR) p.str("state") else "",
            finished = when (word) { "FINISHED" -> true; "STOPPED" -> false; else -> null },
        )
    }

    private fun jobId(): String {
        val id = rest.get("/api/v1/job").obj()?.num("id")
            ?: throw PrinterException(PrinterException.Kind.CONFLICT, "No print in progress")
        return id.toLong().toString()
    }

    override fun pause() {
        rest.put("/api/v1/job/${jobId()}/pause")
    }

    override fun resume() {
        rest.put("/api/v1/job/${jobId()}/resume")
    }

    override fun cancel() {
        rest.delete("/api/v1/job/${jobId()}")
    }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> InputStream) {
        // The storage: the first one the printer lists (USB on the MK4 and the MINI), else «local».
        val storage = runCatching {
            (rest.get("/api/v1/storage").obj()?.get("storage_list") as? JsonArray)?.filterIsInstance<JsonObject>()
                ?.firstOrNull { it.str("available") != "false" && it.str("read_only") != "true" }?.str("path")
        }.getOrNull()?.trim('/')?.ifBlank { null } ?: "local"
        val path = "/api/v1/files/$storage/" + URLEncoder.encode(name, "UTF-8").replace("+", "%20")
        rest.uploadRaw("PUT", path, FormPart("file", "application/octet-stream", name, length, open),
            mapOf("Print-After-Upload" to if (start) "?1" else "?0", "Overwrite" to "?1"))
    }

    override fun camera(): Camera? = null

    private fun unsupported(): Nothing =
        throw PrinterException(PrinterException.Kind.UNSUPPORTED, "PrusaLink has no controls for this")

    override fun setTemperature(heater: Heater, target: Double) = unsupported()
    override fun home(axes: Set<Axis>) = unsupported()
    override fun jog(axis: Axis, distance: Double, feedMmS: Double) = unsupported()
    override fun extrude(mm: Double, feedMmS: Double) = unsupported()

    private fun unexpected() = PrinterException(PrinterException.Kind.UNEXPECTED, "Unexpected answer of PrusaLink")
}
