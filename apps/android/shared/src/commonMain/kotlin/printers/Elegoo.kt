package net.tisma.printers

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.put
import net.tisma.cloud.FormPart
import net.tisma.cloud.Http
import net.tisma.cloud.HttpRequest
import net.tisma.io.ByteInput
import net.tisma.io.BytesInput
import net.tisma.io.IOException
import net.tisma.io.Lock
import net.tisma.io.Md5Digest
import net.tisma.io.hex
import net.tisma.io.nowMs
import net.tisma.io.secureRandomBytes
import net.tisma.io.sleepMs
import net.tisma.io.startDaemon

/**
 * Elegoo printers on the local network over SDCP v3 (printer_connections.md, «Impresoras Elegoo por red local»; the
 * protocol from CBD-Tech's SDCP V3.0.0 and the FDM fields from the community's documentation the spec cites): the
 * Centauri Carbon and the WiFi resin printers with a ChiTu board.
 *
 * - One WebSocket to `ws://host:3030/websocket`, kept open: the printer pushes its state and attributes by itself;
 *   requests carry a `RequestID` its answer repeats. `ping` every 10 s; no `pong` in 20 s and the link is opened again,
 *   the last state kept meanwhile.
 * - Files by HTTP POSTs of 1 MiB to `/uploadFile/upload` on the same port, with the whole file's MD5.
 *
 * Address: `elegoo://host` (tests may give another port: `elegoo://host?port=3030`). Own design over the spec; Android
 * and iOS share it.
 */
class ElegooConnection(
    private val host: String,
    private val port: Int,
    private val http: Http,
    /** How long a request waits for its answer. */
    private val answerMs: Long = 10_000,
    /** How long without a `pong` before the link counts as lost. */
    private val pongMs: Long = 20_000,
) : PrinterConnection {
    private val lock = Lock()
    private var socket: WebSocket? = null
    /** The board's id, from the topic of what it pushes (`sdcp/status/<id>`) or its answers. */
    var mainboardId: String = ""
        private set
    private var lastStatus: JsonObject? = null
    private var attributes: JsonObject? = null
    private var lastPong = 0L
    private val answers = mutableMapOf<String, JsonObject>()
    private var closed = false

    /** The last error the printer pushed (`sdcp/error`): its `ErrorCode`. */
    var lastError: Int? = null
        private set

    private fun link(): WebSocket = lock.locked { socket } ?: open()

    private fun open(): WebSocket {
        val ws = try {
            WebSocket.connect(host, port, "/websocket", readTimeoutMs = (pongMs + 10_000).toInt())
        } catch (e: WebSocket.Refused) {
            if (e.status == 500 && e.body.contains("too many", ignoreCase = true))
                throw PrinterException(PrinterException.Kind.CONFLICT, "The printer has too many connections open.", e,
                    PrinterException.Reason.EL_TOO_MANY)
            throw PrinterException(PrinterException.Kind.NETWORK, e.message ?: "Refused", e, PrinterException.Reason.EL_UNREACHABLE)
        } catch (e: PrinterException) {
            throw PrinterException(PrinterException.Kind.NETWORK, "Could not connect to the printer at $host.", e,
                PrinterException.Reason.EL_UNREACHABLE)
        } catch (e: IOException) {
            throw PrinterException(PrinterException.Kind.NETWORK, "Could not connect to the printer at $host.", e,
                PrinterException.Reason.EL_UNREACHABLE)
        }
        lock.locked { socket = ws; lastPong = nowMs() }
        startDaemon("elegoo-read") { readLoop(ws) }
        startDaemon("elegoo-ping") { pingLoop(ws) }
        // The board's id comes with the first push; asked for if it does not come.
        val end = nowMs() + 3_000
        while (lock.locked { mainboardId.isEmpty() } && nowMs() < end) sleepMs(50)
        if (lock.locked { mainboardId.isEmpty() }) runCatching { request(1) }
        return ws
    }

    private fun drop(ws: WebSocket) {
        lock.locked { if (socket === ws) socket = null }
        runCatching { ws.close() }
    }

    private fun readLoop(ws: WebSocket) {
        try {
            while (true) {
                val text = ws.receive() ?: break
                if (text == "pong") { lock.locked { lastPong = nowMs() }; continue }
                val o = runCatching { Json.parseToJsonElement(text) as? JsonObject }.getOrNull() ?: continue
                handle(o)
            }
        } catch (_: Throwable) {
        } finally {
            drop(ws)
        }
    }

    private fun pingLoop(ws: WebSocket) {
        while (lock.locked { socket === ws && !closed }) {
            runCatching { ws.send("ping") }.onFailure { drop(ws); return }
            sleepMs(PING_MS)
            if (lock.locked { nowMs() - lastPong > pongMs }) { drop(ws); return }
        }
    }

    private fun handle(o: JsonObject) {
        val topic = o.str("Topic")
        val id = topic.substringAfterLast('/', "")
        lock.locked {
            if (id.isNotEmpty() && mainboardId.isEmpty()) mainboardId = id
            when {
                topic.startsWith("sdcp/status/") -> lastStatus = (o["Status"] as? JsonObject) ?: o
                topic.startsWith("sdcp/attributes/") -> attributes = (o["Attributes"] as? JsonObject) ?: o
                topic.startsWith("sdcp/response/") -> {
                    val data = o.o("Data")
                    val rid = data?.str("RequestID").orEmpty()
                    if (data != null && rid.isNotEmpty()) answers[rid] = data
                }
                topic.startsWith("sdcp/error/") -> lastError = find(o, "ErrorCode")?.intOrNull
            }
        }
    }

    /** Sends command [cmd] and waits for its answer's `Data` (with `Ack`); «the printer did not answer» after [answerMs]. */
    private fun request(cmd: Int, data: JsonObject = JsonObject(emptyMap())): JsonObject {
        val ws = link()
        val rid = hex(secureRandomBytes(16))
        val board = lock.locked { mainboardId }
        val body = buildJsonObject {
            put("Id", hex(secureRandomBytes(8)))
            put("Data", buildJsonObject {
                put("Cmd", cmd); put("Data", data); put("RequestID", rid); put("MainboardID", board)
                put("TimeStamp", nowMs() / 1000); put("From", 0)
            })
            put("Topic", "sdcp/request/$board")
        }
        try {
            ws.send(body.toString())
        } catch (e: IOException) {
            drop(ws)
            throw PrinterException(PrinterException.Kind.NETWORK, "The link to the printer was cut.", e, PrinterException.Reason.EL_UNREACHABLE)
        }
        val end = nowMs() + answerMs
        while (nowMs() < end) {
            lock.locked { answers.remove(rid) }?.let { return it }
            sleepMs(20)
        }
        throw PrinterException(PrinterException.Kind.NETWORK, "The printer did not answer.", reason = PrinterException.Reason.EL_NO_ANSWER)
    }

    private fun ack(answer: JsonObject): Int = find(answer, "Ack")?.intOrNull ?: 0

    override fun test(): String {
        link()
        val a = lock.locked { attributes }
        val name = a?.let { find(it, "MachineName")?.contentOrNull ?: find(it, "Name")?.contentOrNull }.orEmpty()
        val fw = a?.let { find(it, "FirmwareVersion")?.contentOrNull }.orEmpty()
        return listOf("Elegoo", name, fw).filter { it.isNotBlank() }.joinToString(" ")
    }

    override fun status(): PrinterStatus {
        // A lost link is opened again; meanwhile the last state is what is shown (EL1.6).
        val fresh = runCatching { request(0); true }.getOrDefault(false)
        val s = lock.locked { lastStatus }
            ?: if (fresh) throw PrinterException(PrinterException.Kind.UNEXPECTED, "No state from the printer")
            else throw PrinterException(PrinterException.Kind.NETWORK, "Could not connect to the printer at $host.",
                reason = PrinterException.Reason.EL_UNREACHABLE)
        return statusOf(s)
    }

    override fun pause() = job(129)

    override fun cancel() = job(130)

    override fun resume() = job(131)

    private fun job(cmd: Int) {
        val a = ack(request(cmd))
        if (a != 0) throw PrinterException(PrinterException.Kind.CONFLICT, "The printer answered $a", reason = PrinterException.Reason.EL_ACK_PRINT + a)
    }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> ByteInput) {
        val file = name.substringAfterLast('/')
        // The whole file's MD5 goes with every piece: read once for it, once for the pieces.
        val md5 = Md5Digest()
        var size = 0L
        open().use { input ->
            val buf = ByteArray(64 * 1024)
            while (true) { val n = input.read(buf, 0, buf.size); if (n < 0) break; md5.update(buf, 0, n); size += n }
        }
        val sum = hex(md5.digest())
        val uuid = hex(secureRandomBytes(16))
        open().use { input ->
            var offset = 0L
            while (offset < size || (size == 0L && offset == 0L)) {
                val piece = readUpTo(input, PIECE)
                val parts = listOf(
                    FormPart.text("S-File-MD5", sum), FormPart.text("Check", "1"), FormPart.text("Offset", offset.toString()),
                    FormPart.text("Uuid", uuid), FormPart.text("TotalSize", size.toString()),
                    FormPart("File", "application/octet-stream", file, piece.size.toLong()) { BytesInput(piece) },
                )
                val r = try {
                    http.send(HttpRequest("POST", "http://$host:$port/uploadFile/upload", emptyMap(), form = parts, timeoutMs = 120_000))
                } catch (e: IOException) {
                    throw PrinterException(PrinterException.Kind.NETWORK, "The upload was cut.", e, PrinterException.Reason.EL_UPLOAD_FAILED)
                }
                val text = r.use { runCatching { it.text() }.getOrDefault("") }
                val answer = runCatching { Json.parseToJsonElement(text) }.getOrNull()
                val code = (answer as? JsonObject)?.let { find(it, "code")?.contentOrNull }
                if (!r.ok || code != "000000") {
                    val why = answer?.let { negative(it) }
                    throw PrinterException(PrinterException.Kind.SERVER, "The upload failed${why?.let { " ($it)" } ?: ""}.",
                        reason = PrinterException.Reason.EL_UPLOAD_FAILED + (why ?: 0))
                }
                offset += piece.size
                if (piece.isEmpty()) break
            }
        }
        if (start) {
            val a = ack(request(128, buildJsonObject { put("Filename", file); put("StartLayer", 0) }))
            if (a != 0) throw PrinterException(if (a == 1) PrinterException.Kind.CONFLICT else PrinterException.Kind.SERVER,
                "The printer did not start the print ($a).", reason = PrinterException.Reason.EL_ACK_PRINT + a)
        }
    }

    private fun readUpTo(input: ByteInput, max: Int): ByteArray {
        val buf = ByteArray(max)
        var n = 0
        while (n < max) { val r = input.read(buf, n, max - n); if (r < 0) break; n += r }
        return buf.copyOf(n)
    }

    override fun camera(): Camera? = null

    override val hasControls: Boolean get() = false

    override fun setTemperature(heater: Heater, target: Double) = unsupported()

    override fun home(axes: Set<Axis>) = unsupported()

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) = unsupported()

    override fun extrude(mm: Double, feedMmS: Double) = unsupported()

    private fun unsupported(): Nothing = throw PrinterException(PrinterException.Kind.UNSUPPORTED, "SDCP has no controls by hand")

    override fun close() {
        val ws = lock.locked { closed = true; socket.also { socket = null } }
        ws?.let { runCatching { it.close() } }
    }

    companion object {
        const val PORT = 3030
        /** The pieces of an upload: 1 MB (1 048 576 bytes, as the spec's offsets show). */
        const val PIECE = 1 shl 20
        const val PING_MS = 10_000L

        fun of(url: String, http: Http): ElegooConnection {
            val rest = url.removePrefix("elegoo://").substringAfter("://")
            val host = rest.substringBefore('?').substringBefore('/').substringBefore(':')
            val port = rest.substringAfter("?port=", "").toIntOrNull() ?: PORT
            return ElegooConnection(host, port, http)
        }

        /** The first value of [key] at any depth (the fields come nested at different depths). */
        fun find(e: JsonElement, key: String): JsonPrimitive? = when (e) {
            is JsonObject -> (e[key] as? JsonPrimitive) ?: e.values.firstNotNullOfOrNull { find(it, key) }
            is JsonArray -> e.firstNotNullOfOrNull { find(it, key) }
            else -> null
        }

        private fun findElement(e: JsonElement, key: String): JsonElement? = when (e) {
            is JsonObject -> e[key] ?: e.values.firstNotNullOfOrNull { findElement(it, key) }
            is JsonArray -> e.firstNotNullOfOrNull { findElement(it, key) }
            else -> null
        }

        /** The upload's errors -1 to -4 anywhere in its answer. */
        private fun negative(e: JsonElement): Int? = when (e) {
            is JsonPrimitive -> e.intOrNull?.takeIf { it in -4..-1 } ?: e.contentOrNull?.toIntOrNull()?.takeIf { it in -4..-1 }
            is JsonObject -> e.values.firstNotNullOfOrNull { negative(it) }
            is JsonArray -> e.firstNotNullOfOrNull { negative(it) }
        }

        /**
         * A pushed `Status`: `CurrentStatus` (0 idle, 1 printing, 2 receiving a file, 3 exposure test, 4 self-check) and
         * `PrintInfo.Status` (5 pausing, 6 paused, 7 stopping, 8 stopped, 9 finished, 10 checking the file). Progress is
         * layers; the ticks' unit is not confirmed, so no times are shown.
         */
        fun statusOf(s: JsonObject): PrinterStatus {
            val current = when (val c = findElement(s, "CurrentStatus")) {
                is JsonArray -> c.mapNotNull { (it as? JsonPrimitive)?.intOrNull }
                is JsonPrimitive -> listOfNotNull(c.intOrNull)
                else -> emptyList()
            }
            val info = (findElement(s, "PrintInfo") as? JsonObject) ?: JsonObject(emptyMap())
            val ps = find(info, "Status")?.intOrNull ?: 0
            val error = find(info, "ErrorNumber")?.intOrNull ?: 0
            val state = when {
                error != 0 -> PrinterState.ERROR
                ps == 5 || ps == 6 -> PrinterState.PAUSED
                ps == 7 || ps == 10 -> PrinterState.BUSY
                ps == 8 || ps == 9 -> PrinterState.READY
                1 in current -> PrinterState.PRINTING
                current.any { it in 2..4 } -> PrinterState.BUSY
                else -> PrinterState.READY
            }
            val layer = find(info, "CurrentLayer")?.intOrNull
            val total = find(info, "TotalLayer")?.intOrNull
            val active = state == PrinterState.PRINTING || state == PrinterState.PAUSED
            fun temp(a: String, t: String) = find(s, a)?.doubleOrNull?.let { Temperature(it, find(s, t)?.doubleOrNull ?: 0.0) }
            return PrinterStatus(
                state = state,
                file = find(info, "Filename")?.contentOrNull.orEmpty(),
                percent = if (active && layer != null && total != null && total > 0) 100.0 * layer / total else null,
                layer = if (active) layer else null,
                totalLayers = if (active) total else null,
                nozzle = temp("TempOfNozzle", "TempTargetNozzle"),
                bed = temp("TempOfHotbed", "TempTargetHotbed"),
                chamber = temp("TempOfBox", "TempTargetBox"),
                message = when (ps) { 5 -> "pausing"; 6 -> "paused"; 7 -> "stopping"; 8 -> "stopped"; 9 -> "finished"; else -> "" },
                finished = when (ps) { 9 -> true; 8 -> false; else -> null },
                reason = if (error != 0) PrinterException.Reason.EL_PRINT_ERROR + error else null,
            )
        }
    }
}

/** Elegoo's discovery answer (UDP `M99999` to port 3000): `Id` and, in `Data`, the name, IP and board id. */
data class ElegooFound(val name: String, val ip: String, val mainboardId: String, val model: String)

object ElegooDiscovery {
    const val PORT = 3000
    const val PROBE = "M99999"

    fun parse(text: String): ElegooFound? {
        val o = runCatching { Json.parseToJsonElement(text) as? JsonObject }.getOrNull() ?: return null
        val d = o.o("Data") ?: o
        val ip = d.str("MainboardIP").ifBlank { return null }
        return ElegooFound(d.str("Name").ifBlank { d.str("MachineName") }, ip, d.str("MainboardID"), d.str("MachineName"))
    }
}
