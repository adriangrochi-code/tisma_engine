package net.tisma.mockprinters

import com.sun.net.httpserver.HttpExchange
import com.sun.net.httpserver.HttpServer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonObject
import java.net.InetSocketAddress
import java.util.concurrent.Executors

/**
 * A simulated printer, shared by the Moonraker and OctoPrint simulators. Each
 * status read moves a print forward by [stepPercent], so tests are
 * deterministic. Written from the public API documentation of each program
 * (printer_connections.md, «Integración en sala limpia», rule 2).
 */
abstract class SimulatedPrinter(private val port: Int, private val host: String) {
    var apiKey: String? = "test-key"
    var stepPercent = 25.0
    /** The board is not connected (OctoPrint answers 409; Klipper is in shutdown). */
    var disconnected = false

    var state = "standby" // standby, printing, paused, complete, cancelled, error
    var file = ""
    var progress = 0.0
    var printSeconds = 0.0
    var nozzle = 24.0
    var nozzleTarget = 0.0
    var bed = 23.0
    var bedTarget = 0.0
    val files = mutableMapOf<String, ByteArray>()
    /** Every request: method and path, for the tests. */
    val log = mutableListOf<Pair<String, String>>()
    /** Every key the printer was sent (to check it never goes elsewhere). */
    val keysSeen = mutableListOf<String?>()

    protected val lock = Any()
    private var server: HttpServer? = null
    val url: String get() = "http://$host:${server!!.address.port}"

    fun start(): SimulatedPrinter {
        val s = HttpServer.create(InetSocketAddress(host, port), 0)
        s.executor = Executors.newFixedThreadPool(4)
        s.createContext("/") { ex -> handle(ex) }
        s.start()
        server = s
        return this
    }

    fun stop() {
        server?.stop(0)
        server = null
    }

    protected class Reply(val status: Int, val body: String, val type: String = "application/json")
    protected class Stop(val status: Int, val body: String) : Exception(body)

    protected abstract fun route(method: String, path: String, query: Map<String, String>, ex: HttpExchange): Reply

    /** The camera: false makes the printer report none. */
    var hasCamera = true

    /** A small JPEG, as a camera snapshot. */
    val snapshot: ByteArray by lazy {
        val img = java.awt.image.BufferedImage(16, 9, java.awt.image.BufferedImage.TYPE_INT_RGB)
        for (x in 0 until 16) for (y in 0 until 9) img.setRGB(x, y, 0x7A24C9)
        java.io.ByteArrayOutputStream().also { javax.imageio.ImageIO.write(img, "jpg", it) }.toByteArray()
    }

    private fun handle(ex: HttpExchange) {
        if (ex.requestURI.rawPath == "/snapshot.jpg") {
            // The camera is served apart from the API, without the key (like a camera streamer).
            ex.responseHeaders.add("Content-Type", "image/jpeg")
            ex.sendResponseHeaders(200, snapshot.size.toLong())
            ex.responseBody.use { it.write(snapshot) }
            return
        }
        val reply = try {
            val key = ex.requestHeaders.getFirst("X-Api-Key")
            synchronized(lock) {
                log += ex.requestMethod to ex.requestURI.rawPath
                keysSeen += key
            }
            if (apiKey != null && key != apiKey) throw Stop(401, unauthorized())
            route(ex.requestMethod, ex.requestURI.rawPath.trimEnd('/'), query(ex.requestURI.rawQuery), ex)
        } catch (s: Stop) {
            Reply(s.status, s.body)
        }
        runCatching { ex.requestBody.readAllBytes() }
        ex.responseHeaders.add("Content-Type", reply.type)
        val bytes = reply.body.toByteArray()
        ex.sendResponseHeaders(reply.status, if (bytes.isEmpty()) -1 else bytes.size.toLong())
        if (bytes.isNotEmpty()) ex.responseBody.use { it.write(bytes) } else ex.close()
    }

    protected abstract fun unauthorized(): String

    /** Moves a print forward: what a status read sees. */
    protected fun tick() = synchronized(lock) {
        if (state == "printing") {
            nozzle = nozzleTarget
            bed = bedTarget
            progress = (progress + stepPercent / 100).coerceAtMost(1.0)
            printSeconds += 60
            if (progress >= 1.0) {
                state = "complete"
                nozzleTarget = 0.0
                bedTarget = 0.0
            }
        }
    }

    protected fun startPrint(name: String) = synchronized(lock) {
        file = name
        state = "printing"
        progress = 0.0
        printSeconds = 0.0
        nozzleTarget = 210.0
        bedTarget = 60.0
    }

    /** The uploaded file of a multipart body, and the other fields as text. */
    protected fun form(ex: HttpExchange): Pair<Pair<String, ByteArray>?, Map<String, String>> {
        val type = ex.requestHeaders.getFirst("Content-Type") ?: ""
        val boundary = Regex("boundary=\"?([^\";]+)\"?").find(type)?.groupValues?.get(1) ?: throw Stop(400, "{}")
        val body = ex.requestBody.readAllBytes()
        val delimiter = "--$boundary".toByteArray()
        var file: Pair<String, ByteArray>? = null
        val fields = mutableMapOf<String, String>()
        var pos = indexOf(body, delimiter, 0)
        while (pos >= 0) {
            val start = pos + delimiter.size
            if (start + 2 <= body.size && body[start] == '-'.code.toByte() && body[start + 1] == '-'.code.toByte()) break
            val headerEnd = indexOf(body, "\r\n\r\n".toByteArray(), start)
            if (headerEnd < 0) break
            val headers = String(body, start, headerEnd - start)
            val next = indexOf(body, "\r\n--$boundary".toByteArray(), headerEnd + 4)
            if (next < 0) break
            val data = body.copyOfRange(headerEnd + 4, next)
            val name = Regex("name=\"([^\"]*)\"").find(headers)?.groupValues?.get(1)
            val filename = Regex("filename=\"([^\"]*)\"").find(headers)?.groupValues?.get(1)
            if (filename != null) file = filename to data else if (name != null) fields[name] = String(data)
            pos = next + 2
        }
        return file to fields
    }

    private fun indexOf(hay: ByteArray, needle: ByteArray, from: Int): Int {
        outer@ for (i in from..hay.size - needle.size) {
            for (j in needle.indices) if (hay[i + j] != needle[j]) continue@outer
            return i
        }
        return -1
    }

    private fun query(q: String?): Map<String, String> = q?.split('&')?.associate {
        val kv = it.split('=', limit = 2)
        java.net.URLDecoder.decode(kv[0], "UTF-8") to java.net.URLDecoder.decode(kv.getOrElse(1) { "" }, "UTF-8")
    } ?: emptyMap()

    protected fun json(e: JsonElement, status: Int = 200) = Reply(status, e.toString())
}

/** Moonraker (Klipper), from moonraker.readthedocs.io. Answers are wrapped in `result`. */
class MockMoonraker(port: Int = 0, host: String = "127.0.0.1") : SimulatedPrinter(port, host) {
    init {
        apiKey = null // Moonraker on a trusted local network needs no key
    }

    override fun unauthorized() = """{"error": {"code": 401, "message": "Unauthorized"}}"""

    private fun ok(e: JsonElement) = json(buildJsonObject { put("result", e) })

    private fun error(code: Int, message: String) =
        Stop(code, buildJsonObject { putJsonObject("error") { put("code", code); put("message", message) } }.toString())

    override fun route(method: String, path: String, query: Map<String, String>, ex: HttpExchange): Reply = when {
        method == "GET" && path == "/server/info" -> ok(buildJsonObject {
            put("klippy_connected", true)
            put("klippy_state", if (disconnected) "shutdown" else "ready")
            put("moonraker_version", "v0.9.3-sim")
        })
        method == "GET" && path == "/printer/objects/query" -> { tick(); ok(objects()) }
        method == "POST" && path == "/printer/print/pause" -> synchronized(lock) {
            if (state != "printing") throw error(400, "Print is not in progress")
            state = "paused"; ok(JsonPrimitive("ok"))
        }
        method == "POST" && path == "/printer/print/resume" -> synchronized(lock) {
            if (state != "paused") throw error(400, "Print is not paused")
            state = "printing"; ok(JsonPrimitive("ok"))
        }
        method == "POST" && path == "/printer/print/cancel" -> synchronized(lock) {
            if (state != "printing" && state != "paused") throw error(400, "No print to cancel")
            state = "cancelled"; nozzleTarget = 0.0; bedTarget = 0.0; ok(JsonPrimitive("ok"))
        }
        method == "POST" && path == "/printer/print/start" -> {
            val name = query["filename"] ?: throw error(400, "filename is required")
            if (name !in files) throw error(404, "File not found")
            startPrint(name); ok(JsonPrimitive("ok"))
        }
        method == "POST" && path == "/server/files/upload" -> {
            val (file, fields) = form(ex)
            val (name, data) = file ?: throw error(400, "No file")
            synchronized(lock) { files[name] = data }
            val printing = fields["print"] == "true"
            if (printing) startPrint(name)
            json(buildJsonObject {
                putJsonObject("item") { put("path", name); put("root", fields["root"] ?: "gcodes") }
                put("print_started", printing)
                put("action", "create_file")
            }, 201)
        }
        method == "GET" && path == "/server/webcams/list" -> ok(buildJsonObject {
            put("webcams", buildJsonArray {
                if (hasCamera) add(buildJsonObject {
                    put("name", "printer"); put("enabled", true)
                    put("stream_url", "/webcam/?action=stream"); put("snapshot_url", "$url/snapshot.jpg")
                })
            })
        })
        else -> throw error(404, "Not Found")
    }

    private fun objects() = synchronized(lock) {
        buildJsonObject {
            put("eventtime", 1234.5)
            putJsonObject("status") {
                putJsonObject("webhooks") {
                    put("state", if (disconnected) "shutdown" else "ready")
                    put("state_message", if (disconnected) "MCU 'mcu' shutdown: Timer too close" else "Printer is ready")
                }
                putJsonObject("print_stats") {
                    put("filename", file); put("state", state); put("print_duration", printSeconds)
                    put("total_duration", printSeconds); put("message", "")
                    putJsonObject("info") { put("current_layer", (progress * 100).toInt()); put("total_layer", 100) }
                }
                putJsonObject("virtual_sdcard") { put("progress", progress); put("is_active", state == "printing") }
                putJsonObject("extruder") { put("temperature", nozzle); put("target", nozzleTarget) }
                putJsonObject("heater_bed") { put("temperature", bed); put("target", bedTarget) }
            }
        }
    }
}

/** OctoPrint, from docs.octoprint.org (REST API). */
class MockOctoPrint(port: Int = 0, host: String = "127.0.0.1") : SimulatedPrinter(port, host) {
    override fun unauthorized() = """{"error": "Invalid API key"}"""

    override fun route(method: String, path: String, query: Map<String, String>, ex: HttpExchange): Reply = when {
        method == "GET" && path == "/api/version" -> json(buildJsonObject {
            put("api", "0.1"); put("server", "1.10.2"); put("text", "OctoPrint 1.10.2")
        })
        method == "GET" && path == "/api/printer" -> {
            if (disconnected) throw Stop(409, """{"error": "Printer is not operational"}""")
            tick()
            json(printer())
        }
        method == "GET" && path == "/api/job" -> json(job())
        method == "POST" && path == "/api/job" -> command(ex)
        method == "POST" && path == "/api/files/local" -> {
            val (file, fields) = form(ex)
            val (name, data) = file ?: throw Stop(400, """{"error": "No file included"}""")
            synchronized(lock) { files[name] = data }
            val printing = fields["print"] == "true"
            if (printing) {
                if (disconnected) throw Stop(409, """{"error": "Printer is not operational"}""")
                startPrint(name)
            }
            json(buildJsonObject {
                putJsonObject("files") { putJsonObject("local") { put("name", name); put("origin", "local") } }
                put("done", true)
            }, 201)
        }
        method == "GET" && path == "/api/settings" -> json(buildJsonObject {
            putJsonObject("webcam") {
                put("webcamEnabled", hasCamera)
                put("streamUrl", "/webcam/?action=stream")
                put("snapshotUrl", "$url/snapshot.jpg")
            }
        })
        else -> throw Stop(404, """{"error": "Not Found"}""")
    }

    private fun command(ex: HttpExchange): Reply {
        val body = Json.parseToJsonElement(ex.requestBody.readAllBytes().toString(Charsets.UTF_8)) as JsonObject
        val cmd = (body["command"] as? JsonPrimitive)?.contentOrNull
        val action = (body["action"] as? JsonPrimitive)?.contentOrNull
        synchronized(lock) {
            when {
                cmd == "pause" && action == "pause" && state == "printing" -> state = "paused"
                cmd == "pause" && action == "resume" && state == "paused" -> state = "printing"
                cmd == "cancel" && (state == "printing" || state == "paused") -> {
                    state = "cancelled"; nozzleTarget = 0.0; bedTarget = 0.0
                }
                cmd == "start" && file.isNotEmpty() && state != "printing" -> startPrint(file)
                else -> throw Stop(409, """{"error": "Printer is not printing"}""")
            }
        }
        return Reply(204, "")
    }

    private fun printer() = synchronized(lock) {
        buildJsonObject {
            putJsonObject("temperature") {
                putJsonObject("tool0") { put("actual", nozzle); put("target", nozzleTarget); put("offset", 0) }
                putJsonObject("bed") { put("actual", bed); put("target", bedTarget); put("offset", 0) }
            }
            putJsonObject("state") {
                put("text", when (state) { "printing" -> "Printing"; "paused" -> "Paused"; else -> "Operational" })
                putJsonObject("flags") {
                    put("operational", true); put("paused", state == "paused"); put("printing", state == "printing")
                    put("pausing", false); put("cancelling", false); put("sdReady", false); put("error", false)
                    put("ready", state != "printing" && state != "paused"); put("closedOrError", false)
                }
            }
        }
    }

    private fun job() = synchronized(lock) {
        val active = state == "printing" || state == "paused"
        buildJsonObject {
            putJsonObject("job") {
                putJsonObject("file") { put("name", file.ifEmpty { null }); put("origin", "local") }
                put("estimatedPrintTime", 400.0)
            }
            putJsonObject("progress") {
                put("completion", if (active || state == "complete") progress * 100 else null)
                put("printTime", if (active) printSeconds else null)
                put("printTimeLeft", if (active) (400 * (1 - progress)) else null)
            }
            put("state", when (state) { "printing" -> "Printing"; "paused" -> "Paused"; else -> "Operational" })
        }
    }
}
