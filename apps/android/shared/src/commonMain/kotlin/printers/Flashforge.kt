package net.tisma.printers

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import net.tisma.cloud.FormPart
import net.tisma.cloud.Http
import net.tisma.io.ByteInput
import net.tisma.io.IOException
import net.tisma.io.Link
import net.tisma.io.nowMs
import net.tisma.io.sleepMs

/**
 * Flashforge printers on the local network (printer_connections.md, «Flashforge en red local», the owner's decision of
 * 17-11-2026; the spec's sources are the community's API documentation and Flashforge's manuals). Own design over the
 * spec; Android and iOS share it.
 *
 * - The modern ones (Adventurer 5M, 5M Pro, AD5X, Creator 5) answer HTTP POSTs on port 8898 with the serial number and
 *   the check code: [FlashforgeHttp].
 * - The older ones (Adventurer 3 and 4) have only TCP on port 8899, with no credentials: [FlashforgeTcp].
 *
 * Address: `flashforge://host` (tests may give other ports: `flashforge://host?http=8898&tcp=8899`). With a check code
 * it is a modern one; without, an older one.
 */
object Flashforge {
    const val HTTP_PORT = 8898
    const val TCP_PORT = 8899

    fun of(url: String, serial: String, checkCode: String, http: Http): PrinterConnection {
        val rest = url.removePrefix("flashforge://").substringAfter("://")
        val host = rest.substringBefore('?').substringBefore('/').trimEnd('/')
        val query = rest.substringAfter('?', "").split('&').mapNotNull { p ->
            p.split('=', limit = 2).takeIf { it.size == 2 }?.let { it[0] to it[1] }
        }.toMap()
        val httpPort = query["http"]?.toIntOrNull() ?: HTTP_PORT
        val tcpPort = query["tcp"]?.toIntOrNull() ?: TCP_PORT
        return if (checkCode.isNotBlank()) FlashforgeHttp(host, httpPort, serial.trim(), checkCode.trim(), http)
        else FlashforgeTcp(host, tcpPort)
    }

    /** What a printer answered to the discovery probe (UDP, any content). */
    data class Found(
        val name: String,
        /** Empty for the older ones: their answer has none (M115 tells it). */
        val serial: String,
        val tcpPort: Int,
        /** 0 for the older ones, which have no HTTP. */
        val httpPort: Int,
        val modern: Boolean,
    )

    /** Where the probe goes: the modern ones on 225.0.0.9:19000 and broadcast :48899, the older ones on 225.0.0.9:8899. */
    const val MULTICAST = "225.0.0.9"
    const val MODERN_MULTICAST_PORT = 19000
    const val MODERN_BROADCAST_PORT = 48899
    const val LEGACY_MULTICAST_PORT = 8899

    /**
     * Reads an answer: 196 bytes or more is a modern one (276 on firmware 3.x and the Creator 5, 280 on 5.x), exactly
     * 140 an older one. Numbers are big-endian; the name (128 bytes from 0) and the serial (128 bytes from 0x92) end at
     * their first zero. The ports are at 0x84 (TCP) and 0x8E (HTTP).
     */
    fun parseAnswer(b: ByteArray): Found? {
        fun u16(at: Int) = if (at + 2 <= b.size) ((b[at].toInt() and 0xFF) shl 8) or (b[at + 1].toInt() and 0xFF) else 0
        fun text(at: Int, max: Int): String {
            if (at >= b.size) return ""
            val end = minOf(b.size, at + max)
            val zero = (at until end).firstOrNull { b[it].toInt() == 0 } ?: end
            return b.copyOfRange(at, zero).decodeToString().trim()
        }
        return when {
            b.size >= 196 -> Found(text(0, 128), text(0x92, 128), u16(0x84).takeIf { it > 0 } ?: TCP_PORT,
                u16(0x8E).takeIf { it > 0 } ?: HTTP_PORT, modern = true)
            b.size == 140 -> Found(text(0, 128), "", u16(0x84).takeIf { it > 0 } ?: TCP_PORT, 0, modern = false)
            else -> null
        }
    }

    /** «No se pudo conectar a la impresora en <IP>…» (the spec's message, with «Solo LAN» for the modern ones). */
    internal fun unreachable(host: String, modern: Boolean, cause: Throwable? = null) = PrinterException(
        PrinterException.Kind.NETWORK,
        "Could not connect to the printer at $host. Check it is on and on the same network" +
            (if (modern) ", and that LAN-only mode is on." else "."),
        cause, if (modern) PrinterException.Reason.FF_UNREACHABLE_LAN else PrinterException.Reason.FF_UNREACHABLE)
}

/**
 * The modern Flashforge printers' HTTP API on port 8898. Every call is a POST; every answer has `code` and `message`.
 * `/control` says «Success» even for a command it does not know, so each pause, resume and cancel is confirmed by
 * reading `/detail` afterwards.
 */
class FlashforgeHttp(
    private val host: String,
    port: Int,
    private val serial: String,
    private val checkCode: String,
    http: Http,
) : PrinterConnection {
    private val rest = Rest("http://$host:$port", emptyMap(), http)

    /** How long a pause, resume or cancel is waited for in `/detail`. */
    var confirmMs = 15_000L

    private fun post(path: String, extra: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}): JsonObject {
        val body = buildJsonObject { put("serialNumber", serial); put("checkCode", checkCode); extra() }
        val r = try {
            rest.post(path, body).obj() ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "No JSON from $path")
        } catch (e: PrinterException) {
            if (e.kind == PrinterException.Kind.NETWORK) throw Flashforge.unreachable(host, true, e) else throw e
        }
        return checked(r)
    }

    private fun checked(r: JsonObject): JsonObject {
        val code = r.num("code")?.toInt() ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "No code in the answer")
        return when (code) {
            0 -> r
            1 -> throw PrinterException(PrinterException.Kind.AUTH,
                "Wrong serial number or check code. Check them on the printer's network mode screen.", reason = PrinterException.Reason.FF_AUTH)
            2 -> throw PrinterException(PrinterException.Kind.CONFLICT, "The printer is busy.", reason = PrinterException.Reason.FF_BUSY)
            3 -> throw PrinterException(PrinterException.Kind.SERVER, "The file is not on the printer.", reason = PrinterException.Reason.FF_NO_FILE)
            -1 -> throw PrinterException(PrinterException.Kind.SERVER, "The printer refused the request.", reason = PrinterException.Reason.FF_REFUSED)
            else -> throw PrinterException(PrinterException.Kind.SERVER, "The printer answered code $code: ${r.str("message")}")
        }
    }

    private fun detail(): JsonObject = post("/detail").o("detail")
        ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "No detail in the answer")

    override fun test(): String {
        val d = detail()
        return listOf("Flashforge", d.str("name"), d.str("firmwareVersion")).filter { it.isNotBlank() }.joinToString(" ")
    }

    override fun status(): PrinterStatus {
        val d = detail()
        val s = d.str("status")
        val state = stateOf(s)
        val active = state == PrinterState.PRINTING || state == PrinterState.PAUSED
        fun temp(a: String, t: String) = d.num(a)?.let { Temperature(it, d.num(t) ?: 0.0) }
        return PrinterStatus(
            state = state,
            file = d.str("printFileName"),
            percent = if (active) d.num("printProgress")?.let { it * 100 } else null,
            elapsedS = if (active) d.num("printDuration")?.toLong() else null,
            // `estimatedTime` is already what is left, in seconds.
            remainingS = if (active) d.num("estimatedTime")?.toLong() else null,
            layer = if (active) d.num("printLayer")?.toInt() else null,
            totalLayers = if (active) d.num("targetPrintLayer")?.toInt() else null,
            nozzle = temp("rightTemp", "rightTargetTemp"),
            bed = temp("platTemp", "platTargetTemp"),
            message = s,
            finished = when (s) { "completed" -> true; "cancel" -> false; else -> null },
        )
    }

    private fun job(action: String, done: (String) -> Boolean) {
        post("/control") {
            put("payload", buildJsonObject {
                put("cmd", "jobCtl_cmd")
                put("args", buildJsonObject { put("jobID", ""); put("action", action) })
            })
        }
        // `/control` answers «Success» for anything: only `/detail` says whether it happened.
        val end = nowMs() + confirmMs
        var last = ""
        while (true) {
            last = detail().str("status")
            if (done(last)) return
            if (nowMs() > end) throw PrinterException(PrinterException.Kind.CONFLICT, "The printer did not $action (it says «$last»).")
            sleepMs(500)
        }
    }

    override fun pause() = job("pause") { it == "pause" || it == "pausing" }

    override fun resume() = job("continue") { it == "printing" }

    override fun cancel() = job("cancel") { it == "cancel" || it == "canceling" || it == "ready" || it == "completed" }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> ByteInput) =
        upload(name, length, start, PrintOptions(), open)

    override fun upload(name: String, length: Long, start: Boolean, options: PrintOptions, open: () -> ByteInput) {
        val file = name.substringAfterLast('/')
        // The size goes in a header: a file of unknown length is measured first.
        val size = if (length >= 0) length else open().use { input ->
            var n = 0L
            val buf = ByteArray(64 * 1024)
            while (true) { val r = input.read(buf, 0, buf.size); if (r < 0) break; n += r }
            n
        }
        val headers = mapOf(
            "serialNumber" to serial,
            "checkCode" to checkCode,
            "fileSize" to size.toString(),
            "printNow" to start.toString(),
            "levelingBeforePrint" to options.bedLeveling.toString(),
            "flowCalibration" to options.flowCalibration.toString(),
            "firstLayerInspection" to "false",
            "timeLapseVideo" to options.timelapse.toString(),
        )
        val r = try {
            rest.upload("/uploadGcode", listOf(FormPart("gcodeFile", "application/octet-stream", file, size, open)), headers).obj()
                ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "No JSON from /uploadGcode")
        } catch (e: PrinterException) {
            if (e.kind == PrinterException.Kind.NETWORK) throw Flashforge.unreachable(host, true, e) else throw e
        }
        checked(r)
    }

    override fun camera(): Camera? = null

    /** The 5M series heats the right nozzle and the plate; -100 turns a heater off and -200 leaves it as it is. */
    override fun setTemperature(heater: Heater, target: Double) {
        val value = if (target <= 0) -100 else target.toInt()
        post("/control") {
            put("payload", buildJsonObject {
                put("cmd", "temperatureCtl_cmd")
                put("args", buildJsonObject {
                    put("rightNozzle", if (heater == Heater.NOZZLE) value else -200)
                    put("leftNozzle", -200)
                    put("platform", if (heater == Heater.BED) value else -200)
                    put("chamber", -200)
                })
            })
        }
    }

    override fun home(axes: Set<Axis>) = unsupported()

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) = unsupported()

    override fun extrude(mm: Double, feedMmS: Double) = unsupported()

    private fun unsupported(): Nothing =
        throw PrinterException(PrinterException.Kind.UNSUPPORTED, "Flashforge's network API has no moving by hand")

    companion object {
        /** `/detail`'s `status`: an unknown value counts as busy. */
        fun stateOf(s: String): PrinterState = when (s) {
            "ready", "completed", "cancel" -> PrinterState.READY
            "printing" -> PrinterState.PRINTING
            "pause", "pausing" -> PrinterState.PAUSED
            "error" -> PrinterState.ERROR
            else -> PrinterState.BUSY
        }
    }
}

/**
 * The older Flashforge printers' TCP protocol on port 8899: commands prefixed with `~`, answers that start with
 * «CMD <code> Received.» and end with «ok». Each use takes the control session (`M601 S1`) and gives it back (`M602`).
 * «Control Failed» means another program holds it, and nothing else is sent. The line ending sent (CR LF) is not
 * confirmed by the sources; the answers end their lines with CR LF.
 */
class FlashforgeTcp(private val host: String, private val port: Int) : PrinterConnection {
    /** The serial number, once M115 told it (the older ones' discovery answer has none). */
    var serial: String = ""
        private set

    private fun <T> session(block: (send: (String) -> List<String>) -> T): T {
        val link = try {
            openTcp(host, port)
        } catch (e: PrinterException) {
            throw Flashforge.unreachable(host, false, e)
        }
        try {
            val lines = LineReader(link)
            fun send(command: String): List<String> {
                val b = "~$command\r\n".encodeToByteArray()
                link.output.write(b, 0, b.size)
                link.output.flush()
                return lines.answer()
            }
            val hello = send("M601 S1")
            if (hello.any { it.contains("Control Failed", ignoreCase = true) })
                throw PrinterException(PrinterException.Kind.CONFLICT, "Another program is controlling the printer.",
                    reason = PrinterException.Reason.FF_OTHER_PROGRAM)
            try {
                return block(::send)
            } finally {
                runCatching { send("M602") }
            }
        } catch (e: IOException) {
            throw PrinterException(PrinterException.Kind.NETWORK, e.message ?: "The printer stopped answering", e)
        } finally {
            runCatching { link.close() }
        }
    }

    /** One answer's lines, up to «ok» (or a refusal of the session, which ends without one). */
    private class LineReader(private val link: Link) {
        fun answer(): List<String> {
            val out = mutableListOf<String>()
            while (true) {
                val line = readLine() ?: break
                out += line
                if (line == "ok" || line.contains("Control Failed", ignoreCase = true)) break
            }
            return out
        }

        private fun readLine(): String? {
            val b = StringBuilder()
            while (true) {
                val c = link.input.read()
                if (c < 0) return if (b.isEmpty()) null else b.toString()
                if (c == '\n'.code) return b.toString().trimEnd('\r').trim()
                b.append(c.toChar())
            }
        }
    }

    private fun field(lines: List<String>, key: String): String =
        lines.firstOrNull { it.startsWith("$key:", ignoreCase = true) }?.substringAfter(':')?.trim().orEmpty()

    override fun test(): String = session { send ->
        val info = send("M115")
        serial = field(info, "SN")
        listOf("Flashforge", field(info, "Machine Type").ifBlank { field(info, "Machine Name") }, field(info, "Firmware"))
            .filter { it.isNotBlank() }.joinToString(" ")
    }

    override fun status(): PrinterStatus = session { send ->
        val st = send("M119")
        val temps = send("M105").firstOrNull { it.contains("T0:") }.orEmpty()
        val machine = field(st, "MachineStatus").uppercase()
        // Only READY is in the sources; the others are read by their words until checked on a printer.
        val state = when {
            machine == "READY" -> PrinterState.READY
            "PAUSE" in machine -> PrinterState.PAUSED
            "BUILDING" in machine || "PRINT" in machine -> PrinterState.PRINTING
            else -> PrinterState.BUSY
        }
        PrinterStatus(state, file = field(st, "CurrentFile"), nozzle = pair(temps, "T0"), bed = pair(temps, "B"), message = machine.lowercase())
    }

    /** `T0:17.9/0.0 B:18.5/0.0`: the actual and target temperature after [key]. */
    private fun pair(line: String, key: String): Temperature? {
        val m = Regex("""(?:^|\s)$key:(-?[\d.]+)/(-?[\d.]+)""").find(line) ?: return null
        return Temperature(m.groupValues[1].toDouble(), m.groupValues[2].toDouble())
    }

    override fun pause() { session { it("M25") } }

    override fun resume() { session { it("M24") } }

    /** After M26 the printer shows a notice that must be closed on its screen before it can be controlled again. */
    override fun cancel() { session { it("M26") } }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> ByteInput): Unit =
        throw PrinterException(PrinterException.Kind.UNSUPPORTED,
            "Sending files to the Adventurer 3 and 4 is not available yet: copy the file with a USB stick.",
            reason = PrinterException.Reason.FF_NO_UPLOAD)

    override fun camera(): Camera? = null

    // The spec lists no heating or moving over this protocol: not offered until it does.
    override val hasControls: Boolean get() = false

    override fun setTemperature(heater: Heater, target: Double) = unsupported()

    override fun home(axes: Set<Axis>) = unsupported()

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) = unsupported()

    override fun extrude(mm: Double, feedMmS: Double) = unsupported()

    private fun unsupported(): Nothing =
        throw PrinterException(PrinterException.Kind.UNSUPPORTED, "Not offered for this Flashforge yet")
}
