package net.tisma.printers

import kotlin.concurrent.Volatile
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.intOrNull
import net.tisma.cloud.FormPart
import net.tisma.cloud.Http
import net.tisma.io.ByteInput
import net.tisma.cloud.percentEncode

/**
 * Duet (RepRapFirmware) over its HTTP requests (the RepRapFirmware wiki,
 * «HTTP requests»; the object model documentation for the fields):
 * `rr_connect` with the board password (a session key from RRF 3.5 on, sent
 * as `X-Session-Key`), `rr_model` for the state, the job and the heaters,
 * `rr_upload` for files and `rr_gcode` for everything else (M32 to print,
 * M25 pause, M24 resume, M0 to cancel once paused; the G-code list of the
 * RepRap wiki).
 */
class DuetConnection(url: String, private val password: String, http: Http) : PrinterConnection {
    private val rest = Rest(url, emptyMap(), http)
    @Volatile private var connected = false

    /** Opens a session (rr_connect); its key, when the firmware gives one, goes with every request. */
    private fun connect(): JsonObject {
        val r = rest.get("/rr_connect?password=" + q(password) + "&sessionKey=yes").obj() ?: throw unexpected()
        when (r.num("err")?.toInt()) {
            0, null -> Unit
            1 -> throw PrinterException(PrinterException.Kind.AUTH, "Wrong board password")
            2 -> throw PrinterException(PrinterException.Kind.CONFLICT, "No free sessions on the board")
            else -> throw unexpected()
        }
        r.num("sessionKey")?.let { rest.extraHeaders = mapOf("X-Session-Key" to it.toLong().toString()) }
            ?: r.str("sessionKey").takeIf { it.isNotBlank() }?.let { rest.extraHeaders = mapOf("X-Session-Key" to it) }
        connected = true
        return r
    }

    /** A request, reconnecting once when the session ended (401). */
    private fun <T> session(block: () -> T): T {
        if (!connected) connect()
        return try {
            block()
        } catch (e: PrinterException) {
            if (e.kind != PrinterException.Kind.AUTH) throw e
            connect()
            block()
        }
    }

    private fun model(key: String, flags: String = "f"): JsonObject? =
        session { rest.get("/rr_model?key=" + q(key) + "&flags=" + q(flags)).obj()?.o("result") }

    private fun modelArray(key: String): List<JsonObject> =
        session { (rest.get("/rr_model?key=" + q(key) + "&flags=f").obj()?.get("result") as? JsonArray) }
            ?.filterIsInstance<JsonObject>() ?: emptyList()

    private fun gcode(code: String) {
        session { rest.get("/rr_gcode?gcode=" + q(code)) }
    }

    override fun test(): String {
        val r = connect()
        return "Duet " + r.str("boardType").ifBlank { "RepRapFirmware" }
    }

    override fun status(): PrinterStatus {
        val st = model("state")
        val job = model("job", "v")
        val heaters = modelArray("heat.heaters")
        val bedIndex = (model("heat", "v")?.get("bedHeaters") as? JsonArray)?.firstOrNull()
            ?.let { (it as? JsonPrimitive)?.intOrNull } ?: -1
        val toolHeater = (modelArray("tools").firstOrNull()?.get("heaters") as? JsonArray)?.firstOrNull()
            ?.let { (it as? JsonPrimitive)?.intOrNull }
        fun heater(i: Int?) = i?.takeIf { it in heaters.indices }?.let { temperature(heaters[it], "current", "active") }
        val status = st?.str("status").orEmpty()
        val state = when (status) {
            "processing", "simulating", "resuming", "cancelling" -> PrinterState.PRINTING
            "paused", "pausing" -> PrinterState.PAUSED
            "idle" -> PrinterState.READY
            "halted", "off" -> PrinterState.ERROR
            "disconnected" -> PrinterState.OFFLINE
            else -> PrinterState.BUSY // busy, changingTool, starting, updating
        }
        val active = state == PrinterState.PRINTING || state == PrinterState.PAUSED
        val file = job?.o("file")
        val size = file?.num("size")
        val pos = job?.num("filePosition")
        val left = job?.o("timesLeft")
        return PrinterStatus(
            state = state,
            file = file?.str("fileName")?.substringAfterLast('/').orEmpty(),
            percent = if (active && size != null && size > 0 && pos != null) 100.0 * pos / size else null,
            elapsedS = if (active) job?.num("duration")?.toLong() else null,
            remainingS = if (active) (left?.num("slicer") ?: left?.num("file"))?.toLong() else null,
            layer = job?.num("layer")?.toInt(),
            totalLayers = file?.num("numLayers")?.toInt()?.takeIf { it > 0 },
            nozzle = heater(toolHeater ?: 1),
            bed = heater(bedIndex),
            message = if (state == PrinterState.ERROR) status else "",
        )
    }

    override fun pause() = gcode("M25")
    override fun resume() = gcode("M24")

    override fun cancel() {
        // A print is cancelled from pause (M0 after M25, the RepRap G-code list).
        if (status().state == PrinterState.PRINTING) gcode("M25")
        gcode("M0")
    }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> ByteInput) {
        val path = "0:/gcodes/" + name.substringAfterLast('/')
        val r = session {
            rest.uploadRaw("POST", "/rr_upload?name=" + q(path), FormPart("file", "application/octet-stream", name, length, open))
        }.obj()
        if (r?.num("err")?.toInt() ?: 0 != 0) throw PrinterException(PrinterException.Kind.SERVER, "The board did not keep the file")
        if (start) gcode("M32 \"$path\"")
    }

    override fun camera(): Camera? = null

    override fun setTemperature(heater: Heater, target: Double) =
        gcode(if (heater == Heater.NOZZLE) "M104 S${num(target)}" else "M140 S${num(target)}")

    override fun home(axes: Set<Axis>) = gcode("G28" + axes.sorted().joinToString("") { " " + it.name })

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) =
        gcode("G91\nG1 ${axis.name}${num(distance)} F${num(feedMmS * 60)}\nG90")

    override fun extrude(mm: Double, feedMmS: Double) = gcode("M83\nG1 E${num(mm)} F${num(feedMmS * 60)}")

    private fun unexpected() = PrinterException(PrinterException.Kind.UNEXPECTED, "Unexpected answer of the Duet")

    private fun q(s: String) = percentEncode(s)

}
