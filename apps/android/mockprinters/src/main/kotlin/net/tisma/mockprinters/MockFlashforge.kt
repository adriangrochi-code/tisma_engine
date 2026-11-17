package net.tisma.mockprinters

import com.sun.net.httpserver.HttpExchange
import com.sun.net.httpserver.HttpServer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.put
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.Executors

/**
 * A Flashforge printer for the tests (printer_connections.md, «Flashforge en red local», FF1): the modern ones' HTTP
 * API on [httpPort] (POSTs with `serialNumber` and `checkCode`, answers with `code` and `message`), the TCP protocol on
 * [tcpPort] (`~` commands, «CMD … Received.» … «ok») and the discovery answers. Written from the spec and its documented
 * sources, as the other simulators.
 */
class MockFlashforge(
    val serial: String = "SNTEST0001",
    val checkCode: String = "0a1b2c3d",
    /** Whether it has HTTP (a modern one) or only TCP (an Adventurer 3 or 4). */
    val modern: Boolean = true,
    http: Int = 0,
    tcp: Int = 0,
) {
    private val loopback = InetAddress.getByName("127.0.0.1")
    private val httpServer: HttpServer? = if (modern) HttpServer.create(InetSocketAddress(loopback, http), 0) else null
    private val tcpServer = ServerSocket(tcp, 10, loopback)
    val httpPort get() = httpServer?.address?.port ?: 0
    val tcpPort get() = tcpServer.localPort
    val url get() = "flashforge://127.0.0.1?http=$httpPort&tcp=$tcpPort"

    @Volatile var status = "ready"
    @Volatile var progress = 0.0
    @Volatile var file = ""
    /** FF1.5: another program holds the TCP control session. */
    @Volatile var controlTaken = false

    /** Every HTTP request: path and JSON body (or, for the upload, its headers). */
    val requests = CopyOnWriteArrayList<Pair<String, JsonObject>>()
    /** Uploads: the form field's name, the file name, its bytes, and the request's headers (lower-case names). */
    val uploads = CopyOnWriteArrayList<Upload>()
    /** Every TCP command, without its `~`. */
    val tcpCommands = CopyOnWriteArrayList<String>()

    class Upload(val field: String, val name: String, val bytes: ByteArray, val headers: Map<String, String>)

    fun start(): MockFlashforge {
        httpServer?.apply {
            executor = Executors.newFixedThreadPool(4)
            createContext("/") { handle(it) }
            start()
        }
        Thread({
            while (true) {
                val s = runCatching { tcpServer.accept() }.getOrNull() ?: break
                Thread({ runCatching { s.use { tcp(it) } } }, "ff-tcp").apply { isDaemon = true }.start()
            }
        }, "ff-tcp-accept").apply { isDaemon = true }.start()
        return this
    }

    fun stop() {
        httpServer?.stop(0)
        runCatching { tcpServer.close() }
        udp.forEach { it.close() }
    }

    /** The answer to a discovery probe: 280 bytes for a modern one (firmware 5.x), 140 for an older one. */
    fun discoveryAnswer(name: String = "Adventurer 5M Pro"): ByteArray {
        val b = java.nio.ByteBuffer.allocate(if (modern) 280 else 140).order(java.nio.ByteOrder.BIG_ENDIAN)
        b.put(name.toByteArray().copyOf(128))
        b.position(0x84)
        b.putShort(tcpPort.toShort())
        b.putShort(0x2B71)
        b.putShort(if (modern) 0x0024 else 0x001D)
        b.putShort(0)
        if (modern) {
            b.putShort(0x5A02)
            b.putShort(httpPort.toShort())
            b.put(0); b.put(0)
            b.put(serial.toByteArray().copyOf(128))
        }
        return b.array()
    }

    /** Answers discovery probes on a UDP port of the loopback (the tests send the probe there). */
    fun answerDiscovery(name: String = "Adventurer 5M Pro"): Int {
        val socket = java.net.DatagramSocket(0, loopback)
        Thread({
            val buf = ByteArray(64)
            while (!socket.isClosed) {
                val p = java.net.DatagramPacket(buf, buf.size)
                runCatching { socket.receive(p) }.getOrNull() ?: break
                val a = discoveryAnswer(name)
                runCatching { socket.send(java.net.DatagramPacket(a, a.size, p.socketAddress)) }
            }
        }, "ff-udp").apply { isDaemon = true }.start()
        udp += socket
        return socket.localPort
    }

    private val udp = CopyOnWriteArrayList<java.net.DatagramSocket>()

    private fun handle(ex: HttpExchange) {
        val path = ex.requestURI.rawPath
        val reply: JsonObject = if (path == "/uploadGcode") upload(ex) else {
            val body = runCatching { Json.parseToJsonElement(String(ex.requestBody.readAllBytes())).jsonObject }.getOrNull()
            if (body == null) answer(-1, "Parameters is error") else {
                requests += path to body
                if (body.s("serialNumber") != serial || body.s("checkCode") != checkCode) answer(1, "SN is different")
                else route(path, body)
            }
        }
        val bytes = reply.toString().toByteArray()
        ex.responseHeaders.add("Content-Type", "application/json")
        ex.sendResponseHeaders(200, bytes.size.toLong())
        ex.responseBody.use { it.write(bytes) }
    }

    private fun JsonObject.s(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull.orEmpty()

    private fun answer(code: Int, message: String, extra: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}) =
        buildJsonObject { put("code", code); put("message", message); extra() }

    private fun route(path: String, body: JsonObject): JsonObject = when (path) {
        "/detail" -> {
            // Each read moves a print forward, as the other simulators.
            if (status == "printing") { progress = (progress + 0.25).coerceAtMost(1.0); if (progress >= 1.0) status = "completed" }
            answer(0, "Success") {
                put("detail", buildJsonObject {
                    put("name", "Adventurer 5M Pro"); put("firmwareVersion", "3.1.5"); put("status", status)
                    put("printProgress", progress); put("printFileName", file)
                    put("printLayer", (progress * 100).toInt()); put("targetPrintLayer", 100)
                    put("printDuration", (progress * 3600).toInt()); put("estimatedTime", (1 - progress) * 3600)
                    put("rightTemp", if (status == "printing") 220.0 else 25.0); put("rightTargetTemp", if (status == "printing") 220.0 else 0.0)
                    put("platTemp", if (status == "printing") 60.0 else 24.0); put("platTargetTemp", if (status == "printing") 60.0 else 0.0)
                })
            }
        }
        "/control" -> {
            val payload = body["payload"] as? JsonObject
            val args = payload?.get("args") as? JsonObject
            if (payload?.s("cmd") == "jobCtl_cmd") when (args?.s("action")) {
                "pause" -> if (status == "printing") status = "pause"
                "continue" -> if (status == "pause") status = "printing"
                "cancel" -> if (status == "printing" || status == "pause") status = "cancel"
            }
            // Unknown commands answer «Success» too, as the documentation warns.
            answer(0, "Success")
        }
        "/printGcode" -> if (body.s("fileName").isEmpty() || body["levelingBeforePrint"] == null) answer(-1, "Parameters is error")
            else if (uploads.none { it.name == body.s("fileName") }) answer(3, "File not exist") else { begin(body.s("fileName")); answer(0, "Success") }
        else -> answer(-1, "Parameters is error")
    }

    private fun begin(name: String) { file = name; status = "printing"; progress = 0.0 }

    private fun upload(ex: HttpExchange): JsonObject {
        val headers = ex.requestHeaders.entries.associate { it.key.lowercase() to it.value.first() }
        requests += "/uploadGcode" to buildJsonObject { headers.forEach { (k, v) -> put(k, v) } }
        if (headers["serialnumber"] != serial || headers["checkcode"] != checkCode) return answer(1, "Access code is different")
        val type = headers["content-type"].orEmpty()
        val boundary = Regex("boundary=\"?([^\";]+)\"?").find(type)?.groupValues?.get(1) ?: return answer(-1, "Parameters is error")
        val body = ex.requestBody.readAllBytes()
        val text = String(body, Charsets.ISO_8859_1)
        val start = text.indexOf("--$boundary")
        val headEnd = text.indexOf("\r\n\r\n", start)
        val end = text.indexOf("\r\n--$boundary", headEnd)
        if (start < 0 || headEnd < 0 || end < 0) return answer(-1, "Parameters is error")
        val partHead = text.substring(start, headEnd)
        val field = Regex("name=\"([^\"]*)\"").find(partHead)?.groupValues?.get(1).orEmpty()
        val name = Regex("filename=\"([^\"]*)\"").find(partHead)?.groupValues?.get(1).orEmpty()
        val bytes = body.copyOfRange(headEnd + 4, end)
        uploads += Upload(field, name, bytes, headers)
        if (headers["printnow"] == "true") begin(name)
        return answer(0, "Success")
    }

    private fun tcp(s: Socket) {
        val r = BufferedReader(InputStreamReader(s.getInputStream(), Charsets.US_ASCII))
        val w = s.getOutputStream()
        fun say(vararg lines: String) { w.write(lines.joinToString("") { "$it\r\n" }.toByteArray()); w.flush() }
        var control = false
        while (true) {
            val line = r.readLine()?.trim() ?: break
            if (!line.startsWith("~")) continue
            val cmd = line.removePrefix("~")
            tcpCommands += cmd
            val code = cmd.substringBefore(' ')
            if (code != "M601" && !control) { say("CMD $code Received.", "Control Failed."); continue }
            when (code) {
                "M601" -> if (controlTaken) say("CMD M601 Received.", "Control Failed.") else { control = true; say("CMD M601 Received.", "Control Success V2.1.", "ok") }
                "M602" -> { control = false; say("CMD M602 Received.", "Control Release.", "ok") }
                "M115" -> say("CMD M115 Received.", "Machine Type: Flashforge Adventurer 4", "Machine Name: Adventurer 4", "Firmware: v2.2.0",
                    "SN: $serial", "X: 220 Y: 200 Z: 250", "Tool Count: 1", "Mac Address:00:11:22:33:44:55", "ok")
                "M119" -> say("CMD M119 Received.", "Endstop: X-max: 110 Y-max: 110 Z-min: 0",
                    "MachineStatus: " + when (status) { "printing" -> "BUILDING_FROM_SD"; "pause" -> "PAUSED"; else -> "READY" },
                    "MoveMode: READY", "Status: S:1 L:0 J:0 F:0", "LED: 1", "CurrentFile:$file", "ok")
                "M105" -> say("CMD M105 Received.", "T0:${if (status == "printing") "210.0/210.0" else "17.9/0.0"} T1:0.0/0.0 B:18.5/0.0", "ok")
                "M25" -> { if (status == "printing") status = "pause"; say("CMD M25 Received.", "ok") }
                // Some firmware ends with «ok.»: the client takes both.
                "M24" -> { if (status == "pause") status = "printing"; say("CMD M24 Received.", "ok.") }
                "M26" -> { status = "ready"; say("CMD M26 Received.", "ok") }
                else -> say("CMD $code Received.", "ok")
            }
        }
    }
}
