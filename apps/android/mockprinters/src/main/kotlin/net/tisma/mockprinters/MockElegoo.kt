package net.tisma.mockprinters

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import java.io.DataInputStream
import java.io.OutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicInteger

/**
 * An Elegoo printer over SDCP v3 for the tests (printer_connections.md, «Impresoras Elegoo por red local», EL1): one
 * port with the WebSocket at `/websocket` (requests with `RequestID`, pushed `sdcp/status` and `sdcp/attributes`,
 * `ping`/`pong`) and the 1 MB uploads at `/uploadFile/upload`, and the UDP answer to `M99999`. Written from the spec.
 */
class MockElegoo(val mainboardId: String = "ELEGOO0000000001", val name: String = "Centauri Carbon", port: Int = 0,
                 /** The model in the attributes: «Saturn 3 Ultra» makes it a resin printer. */
                 val machine: String = "Centauri Carbon") {
    private val loopback = InetAddress.getByName("127.0.0.1")
    private val server = ServerSocket(port, 20, loopback)
    val port get() = server.localPort
    val url get() = "elegoo://127.0.0.1?port=$port"

    /** `CurrentStatus` and `PrintInfo.Status`, as the spec numbers them. */
    @Volatile var current = 0
    @Volatile var printStatus = 0
    @Volatile var filename = ""
    @Volatile var layer = 0
    /** EL1.4: the next start answers Ack 1 (busy). */
    @Volatile var busy = false
    /** EL1.3: the upload piece (0-based) that answers «111111» with -2. */
    @Volatile var failPiece = -1
    /** EL1.6: no `pong` any more. */
    @Volatile var silent = false
    /** EL1.6: WebSockets accepted before a 500 «too many client». */
    @Volatile var maxClients = 5

    class Piece(val offset: Long, val uuid: String, val totalSize: Long, val md5: String, val check: String, val bytes: ByteArray, val filename: String)
    val pieces = CopyOnWriteArrayList<Piece>()
    /** Requests received: `Cmd` and `RequestID`; and the `RequestID`s answered. */
    val requests = CopyOnWriteArrayList<Pair<Int, String>>()
    val answered = CopyOnWriteArrayList<String>()
    /** Every `PrintInfo.Status` pushed, in order. */
    val pushed = CopyOnWriteArrayList<Int>()
    val clients = AtomicInteger()
    val pings = AtomicInteger()
    private val outs = CopyOnWriteArrayList<OutputStream>()
    private val udp = CopyOnWriteArrayList<java.net.DatagramSocket>()

    fun start(): MockElegoo {
        Thread({
            while (true) {
                val s = runCatching { server.accept() }.getOrNull() ?: break
                Thread({ runCatching { s.use { serve(it) } } }, "elegoo-conn").apply { isDaemon = true }.start()
            }
        }, "elegoo-accept").apply { isDaemon = true }.start()
        return this
    }

    fun stop() {
        runCatching { server.close() }
        udp.forEach { it.close() }
    }

    /** Cuts every WebSocket (a Wi-Fi drop). */
    fun cut() = outs.forEach { runCatching { it.close() } }

    /** Answers `M99999` on a UDP port of the loopback; [times] answers per probe (EL1.1 sends the same twice). */
    fun answerDiscovery(times: Int = 1): Int {
        val socket = java.net.DatagramSocket(0, loopback)
        Thread({
            val buf = ByteArray(64)
            while (!socket.isClosed) {
                val p = java.net.DatagramPacket(buf, buf.size)
                runCatching { socket.receive(p) }.getOrNull() ?: break
                if (String(p.data, 0, p.length) != "M99999") continue
                val a = buildJsonObject {
                    put("Id", "x")
                    put("Data", buildJsonObject {
                        put("Name", name); put("MachineName", "Centauri Carbon"); put("BrandName", "ELEGOO")
                        put("MainboardIP", "127.0.0.1"); put("MainboardID", mainboardId); put("ProtocolVersion", "V3.0.0")
                        put("FirmwareVersion", "V1.1.29")
                    })
                }.toString().toByteArray()
                repeat(times) { runCatching { socket.send(java.net.DatagramPacket(a, a.size, p.socketAddress)) } }
            }
        }, "elegoo-udp").apply { isDaemon = true }.start()
        udp += socket
        return socket.localPort
    }

    private fun serve(s: Socket) {
        val input = DataInputStream(s.getInputStream().buffered())
        val head = StringBuilder()
        while (!head.endsWith("\r\n\r\n")) { val b = input.read(); if (b < 0) return; head.append(b.toChar()) }
        val lines = head.toString().split("\r\n")
        val (method, path) = lines[0].split(' ').let { it[0] to it[1] }
        val headers = lines.drop(1).filter { ':' in it }.associate { it.substringBefore(':').trim().lowercase() to it.substringAfter(':').trim() }
        val out = s.getOutputStream()
        if (headers["upgrade"].equals("websocket", true) && path == "/websocket") {
            if (clients.get() >= maxClients) {
                out.write("HTTP/1.1 500 Internal Server Error\r\nContent-Length: 15\r\nConnection: close\r\n\r\ntoo many client".toByteArray())
                out.flush(); return
            }
            val accept = java.util.Base64.getEncoder().encodeToString(java.security.MessageDigest.getInstance("SHA-1")
                .digest((headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").toByteArray()))
            out.write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: $accept\r\n\r\n".toByteArray())
            out.flush()
            clients.incrementAndGet()
            outs += out
            try {
                // On connecting, the printer pushes its attributes and state.
                send(out, attributes()); send(out, status())
                websocket(input, out)
            } finally {
                outs -= out
                clients.decrementAndGet()
            }
        } else if (method == "GET" && path == "/_tisma/received") {
            // For the release gate (G16): each uploaded file whole, and whether it prints.
            val files = pieces.map { it.uuid }.distinct().associate { u -> pieces.first { it.uuid == u }.filename to file(u) }
            val reply = buildJsonObject {
                put("state", if (current == 1) "printing" else "idle")
                put("file", filename)
                put("files", buildJsonObject {
                    files.forEach { (n, b) -> put(n, buildJsonObject {
                        put("size", b.size)
                        put("sha256", java.security.MessageDigest.getInstance("SHA-256").digest(b).joinToString("") { "%02x".format(it) })
                    }) }
                })
            }.toString().toByteArray()
            out.write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${reply.size}\r\nConnection: close\r\n\r\n".toByteArray())
            out.write(reply); out.flush()
        } else if (method == "POST" && path == "/uploadFile/upload") {
            val body = ByteArray(headers["content-length"]?.toInt() ?: 0).also { input.readFully(it) }
            val reply = upload(headers["content-type"].orEmpty(), body)
            out.write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${reply.size}\r\nConnection: close\r\n\r\n".toByteArray())
            out.write(reply); out.flush()
        } else {
            out.write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n".toByteArray()); out.flush()
        }
    }

    private fun upload(type: String, body: ByteArray): ByteArray {
        val boundary = Regex("boundary=\"?([^\";]+)\"?").find(type)?.groupValues?.get(1) ?: return fail(-4)
        val text = String(body, Charsets.ISO_8859_1)
        val fields = mutableMapOf<String, String>()
        var bytes = ByteArray(0); var filename = ""
        var pos = text.indexOf("--$boundary")
        while (pos >= 0) {
            val headEnd = text.indexOf("\r\n\r\n", pos)
            val next = text.indexOf("\r\n--$boundary", headEnd)
            if (headEnd < 0 || next < 0) break
            val partHead = text.substring(pos, headEnd)
            val name = Regex("name=\"([^\"]*)\"").find(partHead)?.groupValues?.get(1).orEmpty()
            val fn = Regex("filename=\"([^\"]*)\"").find(partHead)?.groupValues?.get(1)
            if (fn != null) { bytes = body.copyOfRange(headEnd + 4, next); filename = fn } else fields[name] = text.substring(headEnd + 4, next)
            pos = text.indexOf("--$boundary", next + 2).takeIf { text.startsWith("--$boundary--", next + 2).not() } ?: -1
        }
        val index = pieces.size
        val offset = fields["Offset"]?.toLongOrNull() ?: return fail(-1)
        pieces += Piece(offset, fields["Uuid"].orEmpty(), fields["TotalSize"]?.toLongOrNull() ?: -1, fields["S-File-MD5"].orEmpty(),
            fields["Check"].orEmpty(), bytes, filename)
        if (index == failPiece) return fail(-2)
        val expected = pieces.dropLast(1).filter { it.uuid == fields["Uuid"] }.sumOf { it.bytes.size.toLong() }
        if (offset != expected) return fail(-2)
        return """{"code":"000000","messages":null,"data":{},"success":true}""".toByteArray()
    }

    private fun fail(code: Int) = """{"code":"111111","messages":[{"field":"common_field","message":$code}],"data":{},"success":false}""".toByteArray()

    /** The whole uploaded file of [uuid], in order. */
    fun file(uuid: String = pieces.lastOrNull()?.uuid.orEmpty()): ByteArray =
        pieces.filter { it.uuid == uuid }.sortedBy { it.offset }.fold(ByteArray(0)) { a, p -> a + p.bytes }

    private fun websocket(input: DataInputStream, out: OutputStream) {
        while (true) {
            val b0 = input.read(); if (b0 < 0) return
            val b1 = input.read()
            var len = (b1 and 0x7F).toLong()
            if (len == 126L) len = input.readUnsignedShort().toLong() else if (len == 127L) len = input.readLong()
            val mask = ByteArray(4).also { if (b1 and 0x80 != 0) input.readFully(it) }
            val payload = ByteArray(len.toInt()).also { input.readFully(it) }
            for (i in payload.indices) payload[i] = (payload[i].toInt() xor mask[i % 4].toInt()).toByte()
            when (b0 and 0x0F) {
                0x8 -> return
                0x1 -> {
                    val text = String(payload)
                    if (text == "ping") { pings.incrementAndGet(); if (!silent) frame(out, "pong"); continue }
                    val o = runCatching { Json.parseToJsonElement(text) as JsonObject }.getOrNull() ?: continue
                    request(o, out)
                }
            }
        }
    }

    private fun request(o: JsonObject, out: OutputStream) {
        val d = o["Data"] as? JsonObject ?: return
        val cmd = d["Cmd"]!!.jsonPrimitive.int
        val rid = (d["RequestID"] as? JsonPrimitive)?.contentOrNull.orEmpty()
        requests += cmd to rid
        var ack = 0
        val after = mutableListOf<() -> Unit>()
        when (cmd) {
            0 -> after += { send(out, status()) }
            1 -> after += { send(out, attributes()) }
            128 -> if (busy) ack = 1 else {
                val f = ((d["Data"] as? JsonObject)?.get("Filename") as? JsonPrimitive)?.contentOrNull.orEmpty()
                if (pieces.none { it.filename == f }) ack = 2 else { filename = f; current = 1; printStatus = 3; layer = 0; after += { broadcast() } }
            }
            129 -> after += { printStatus = 5; broadcast(); printStatus = 6; broadcast() }
            130 -> after += { printStatus = 7; broadcast(); printStatus = 8; current = 0; broadcast() }
            131 -> after += { printStatus = 3; current = 1; broadcast() }
        }
        send(out, buildJsonObject {
            put("Id", "x")
            put("Data", buildJsonObject {
                put("Cmd", cmd); put("Data", buildJsonObject { put("Ack", ack) }); put("RequestID", rid)
                put("MainboardID", mainboardId); put("TimeStamp", System.currentTimeMillis() / 1000)
            })
            put("Topic", "sdcp/response/$mainboardId")
        })
        answered += rid
        after.forEach { it() }
    }

    private fun broadcast() { pushed += printStatus; outs.forEach { runCatching { send(it, status()) } } }

    private fun status() = buildJsonObject {
        put("Status", buildJsonObject {
            put("CurrentStatus", buildJsonArray { add(JsonPrimitive(current)) })
            put("TempOfNozzle", if (current == 1) 220.0 else 25.0); put("TempTargetNozzle", if (current == 1) 220.0 else 0.0)
            put("TempOfHotbed", if (current == 1) 60.0 else 24.0); put("TempTargetHotbed", if (current == 1) 60.0 else 0.0)
            put("CurrenCoord", "0.00,0.00,0.00")
            put("PrintInfo", buildJsonObject {
                put("Status", printStatus); put("CurrentLayer", layer); put("TotalLayer", 100)
                put("CurrentTicks", 0); put("TotalTicks", 0); put("Filename", filename); put("ErrorNumber", 0); put("TaskId", "")
            })
        })
        put("MainboardID", mainboardId)
        put("TimeStamp", System.currentTimeMillis() / 1000)
        put("Topic", "sdcp/status/$mainboardId")
    }

    private fun attributes() = buildJsonObject {
        put("Attributes", buildJsonObject {
            put("Name", name); put("MachineName", machine); put("BrandName", "ELEGOO")
            put("FirmwareVersion", "V1.1.29"); put("MainboardID", mainboardId); put("XYZsize", "256x256x256")
        })
        put("MainboardID", mainboardId)
        put("Topic", "sdcp/attributes/$mainboardId")
    }

    private fun send(out: OutputStream, o: JsonObject) = frame(out, o.toString())

    private fun frame(out: OutputStream, text: String) = synchronized(out) {
        val p = text.toByteArray()
        out.write(0x81)
        when {
            p.size < 126 -> out.write(p.size)
            p.size < 65536 -> { out.write(126); out.write(p.size shr 8); out.write(p.size and 0xFF) }
            else -> { out.write(127); for (i in 7 downTo 0) out.write(((p.size.toLong() ushr (8 * i)) and 0xFF).toInt()) }
        }
        out.write(p); out.flush()
    }
}
