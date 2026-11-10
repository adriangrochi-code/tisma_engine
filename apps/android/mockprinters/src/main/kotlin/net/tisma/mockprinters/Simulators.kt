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
    val files = java.util.concurrent.ConcurrentHashMap<String, ByteArray>()
    /** Every request: method and path, for the tests. */
    val log = java.util.concurrent.CopyOnWriteArrayList<Pair<String, String>>()
    /** The G-code lines and commands the printer was given by hand (controls), for the tests. */
    val commands = java.util.concurrent.CopyOnWriteArrayList<String>()
    var position = doubleArrayOf(0.0, 0.0, 0.0)
    var homed = false

    /** Every key the printer was sent (to check it never goes elsewhere). */
    val keysSeen = java.util.concurrent.CopyOnWriteArrayList<String?>()

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

    /** How the camera is mounted, as the printer's settings say: mirrored, and turned clockwise (0 or 270 here). */
    var cameraFlipped = false
    var cameraRotation = 0

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
            if (!files.containsKey(name)) throw error(404, "File not found")
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
        method == "POST" && path == "/printer/gcode/script" -> {
            val script = query["script"] ?: throw error(400, "script is required")
            synchronized(lock) {
                for (line in script.lines().map { it.trim() }.filter { it.isNotEmpty() }) {
                    if ((state == "printing" || state == "paused") && !line.startsWith("M104") && !line.startsWith("M140"))
                        throw error(400, "Printer is busy")
                    if (line.startsWith("G1") && line.contains(" E") && nozzle < 170)
                        throw error(400, "Extrude below minimum temp")
                    when {
                        line.startsWith("M104") -> { nozzleTarget = line.substringAfter("S").toDouble(); nozzle = nozzleTarget.coerceAtLeast(24.0) }
                        line.startsWith("M140") -> { bedTarget = line.substringAfter("S").toDouble(); bed = bedTarget.coerceAtLeast(23.0) }
                        line.startsWith("G28") -> homed = true
                    }
                    commands += line
                }
            }
            ok(JsonPrimitive("ok"))
        }
        method == "GET" && path == "/server/webcams/list" -> ok(buildJsonObject {
            put("webcams", buildJsonArray {
                if (hasCamera) add(buildJsonObject {
                    put("name", "printer"); put("enabled", true)
                    put("stream_url", "/webcam/?action=stream"); put("snapshot_url", "$url/snapshot.jpg")
                    put("flip_horizontal", cameraFlipped); put("flip_vertical", false); put("rotation", cameraRotation)
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
        method == "POST" && path in setOf("/api/printer/tool", "/api/printer/bed", "/api/printer/printhead") -> {
            val body = Json.parseToJsonElement(ex.requestBody.readBytes().toString(Charsets.UTF_8)) as JsonObject
            val cmd = (body["command"] as? JsonPrimitive)?.contentOrNull
            synchronized(lock) {
                val busy = state == "printing" || state == "paused"
                when {
                    path == "/api/printer/bed" && cmd == "target" -> {
                        bedTarget = (body["target"] as JsonPrimitive).content.toDouble(); bed = bedTarget.coerceAtLeast(23.0)
                    }
                    path == "/api/printer/tool" && cmd == "target" -> {
                        nozzleTarget = ((body["targets"] as JsonObject)["tool0"] as JsonPrimitive).content.toDouble()
                        nozzle = nozzleTarget.coerceAtLeast(24.0)
                    }
                    busy -> throw Stop(409, """{"error": "Printer is currently printing"}""")
                    path == "/api/printer/tool" && cmd == "extrude" -> if (nozzle < 170) throw Stop(409, """{"error": "Tool is too cold"}""")
                    path == "/api/printer/printhead" && cmd == "home" -> homed = true
                    path == "/api/printer/printhead" && cmd == "jog" -> Unit
                    else -> throw Stop(400, """{"error": "Unknown command"}""")
                }
                commands += "$path $cmd ${body}"
            }
            Reply(204, "")
        }
        method == "GET" && path == "/api/settings" -> json(buildJsonObject {
            putJsonObject("webcam") {
                put("webcamEnabled", hasCamera)
                put("streamUrl", "/webcam/?action=stream")
                put("snapshotUrl", "$url/snapshot.jpg")
                put("flipH", cameraFlipped); put("flipV", false); put("rotate90", cameraRotation == 270)
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

/**
 * PrusaLink, from its published OpenAPI: HTTP Digest (RFC 7616, MD5, qop auth)
 * checked here like a printer would, v1 status, job, files by PUT.
 */
class MockPrusaLink(port: Int = 0, host: String = "127.0.0.1") : SimulatedPrinter(port, host) {
    var user = "maker"
    var password = "test-pass"
    private val realm = "Printer API"
    private val nonce = java.util.UUID.randomUUID().toString().replace("-", "")
    private var jobId = 0

    init {
        apiKey = null // Digest instead
    }

    override fun unauthorized() = ""

    private fun md5(s: String) = java.security.MessageDigest.getInstance("MD5").digest(s.toByteArray())
        .joinToString("") { "%02x".format(it) }

    private fun checkDigest(ex: HttpExchange) {
        val h = ex.requestHeaders.getFirst("Authorization")
        val challenge = "Digest realm=\"$realm\", nonce=\"$nonce\", qop=\"auth\", algorithm=MD5"
        if (h == null || !h.startsWith("Digest ")) {
            ex.responseHeaders.add("WWW-Authenticate", challenge); throw Stop(401, "")
        }
        val p = Regex("""(\w+)\s*=\s*(?:"([^"]*)"|([^\s,]+))""").findAll(h.substring(7))
            .associate { it.groupValues[1] to it.groupValues[2].ifEmpty { it.groupValues[3] } }
        val ha1 = md5("${p["username"]}:$realm:$password")
        val ha2 = md5("${ex.requestMethod}:${p["uri"]}")
        val expected = md5("$ha1:${p["nonce"]}:${p["nc"]}:${p["cnonce"]}:${p["qop"]}:$ha2")
        if (p["username"] != user || p["nonce"] != nonce || p["response"] != expected ||
            p["uri"] != ex.requestURI.rawPath + (ex.requestURI.rawQuery?.let { "?$it" } ?: "")) {
            ex.responseHeaders.add("WWW-Authenticate", challenge); throw Stop(401, "")
        }
    }

    override fun route(method: String, path: String, query: Map<String, String>, ex: HttpExchange): Reply {
        checkDigest(ex)
        return when {
            method == "GET" && path == "/api/version" -> json(buildJsonObject {
                put("api", "2.0.0"); put("version", "0.8.0"); put("printer", "1.3.0"); put("text", "PrusaLink"); put("firmware", "6.1.0")
            })
            method == "GET" && path == "/api/v1/status" -> { tick(); json(status()) }
            method == "GET" && path == "/api/v1/job" -> synchronized(lock) {
                if (state != "printing" && state != "paused") Reply(204, "") else json(buildJsonObject {
                    put("id", jobId); put("state", state.uppercase()); put("progress", progress * 100)
                    putJsonObject("file") { put("name", file.take(8)); put("display_name", file) }
                })
            }
            method == "GET" && path == "/api/v1/storage" -> json(buildJsonObject {
                put("storage_list", buildJsonArray {
                    add(buildJsonObject { put("path", "/usb/"); put("available", true); put("read_only", false) })
                })
            })
            method == "PUT" && path.startsWith("/api/v1/files/usb/") -> {
                val len = ex.requestHeaders.getFirst("Content-Length") ?: throw Stop(411, "")
                val name = java.net.URLDecoder.decode(path.removePrefix("/api/v1/files/usb/"), "UTF-8")
                val data = ex.requestBody.readBytes()
                if (data.size != len.toInt()) throw Stop(400, "")
                synchronized(lock) { files[name] = data }
                if (ex.requestHeaders.getFirst("Print-After-Upload") == "?1") { startPrint(name); synchronized(lock) { jobId++ } }
                Reply(201, "")
            }
            method == "PUT" && path == "/api/v1/job/$jobId/pause" -> synchronized(lock) {
                if (state != "printing") throw Stop(409, ""); state = "paused"; Reply(204, "")
            }
            method == "PUT" && path == "/api/v1/job/$jobId/resume" -> synchronized(lock) {
                if (state != "paused") throw Stop(409, ""); state = "printing"; Reply(204, "")
            }
            method == "DELETE" && path == "/api/v1/job/$jobId" -> synchronized(lock) {
                if (state != "printing" && state != "paused") throw Stop(409, "")
                state = "cancelled"; nozzleTarget = 0.0; bedTarget = 0.0; Reply(204, "")
            }
            else -> throw Stop(404, "")
        }
    }

    private fun status() = synchronized(lock) {
        buildJsonObject {
            putJsonObject("printer") {
                put("state", when (state) { "printing" -> "PRINTING"; "paused" -> "PAUSED"; "complete" -> "FINISHED"; "cancelled" -> "STOPPED"; else -> "IDLE" })
                put("temp_nozzle", nozzle); put("target_nozzle", nozzleTarget); put("temp_bed", bed); put("target_bed", bedTarget)
            }
            if (state == "printing" || state == "paused") putJsonObject("job") {
                put("id", jobId); put("progress", progress * 100); put("time_printing", printSeconds.toInt())
                put("time_remaining", (400 * (1 - progress)).toInt())
            }
        }
    }
}

/** A Duet (RepRapFirmware): rr_connect with a session key, rr_model, rr_upload, rr_gcode. */
class MockDuet(port: Int = 0, host: String = "127.0.0.1") : SimulatedPrinter(port, host) {
    var password = "reprap"
    private val sessionKey = (100000..999999).random().toString()
    private var size = 1

    init {
        apiKey = null // a session instead
    }

    override fun unauthorized() = "{}"

    override fun route(method: String, path: String, query: Map<String, String>, ex: HttpExchange): Reply {
        if (path == "/rr_connect") {
            return if (query["password"] != password) json(buildJsonObject { put("err", 1) })
            else json(buildJsonObject { put("err", 0); put("sessionTimeout", 8000); put("boardType", "duet3mb6hc"); put("sessionKey", sessionKey.toLong()) })
        }
        if (ex.requestHeaders.getFirst("X-Session-Key") != sessionKey) throw Stop(401, "{}")
        return when (path) {
            "/rr_model" -> { if (query["key"] == "state") tick(); json(buildJsonObject { put("key", query["key"] ?: ""); put("result", model(query["key"] ?: "")) }) }
            "/rr_upload" -> {
                val name = query["name"] ?: throw Stop(400, "{}")
                val data = ex.requestBody.readBytes()
                synchronized(lock) { files[name] = data; size = data.size.coerceAtLeast(1) }
                json(buildJsonObject { put("err", 0) })
            }
            "/rr_gcode" -> {
                val code = query["gcode"] ?: throw Stop(400, "{}")
                synchronized(lock) {
                    for (line in code.lines().map { it.trim() }.filter { it.isNotEmpty() }) {
                        commands += line
                        when {
                            line.startsWith("M32") -> startPrint(line.substringAfter('"').substringBefore('"'))
                            line == "M25" && state == "printing" -> state = "paused"
                            line == "M24" && state == "paused" -> state = "printing"
                            line == "M0" && state == "paused" -> { state = "cancelled"; nozzleTarget = 0.0; bedTarget = 0.0 }
                            line.startsWith("M104") -> { nozzleTarget = line.substringAfter("S").toDouble(); nozzle = nozzleTarget.coerceAtLeast(24.0) }
                            line.startsWith("M140") -> { bedTarget = line.substringAfter("S").toDouble(); bed = bedTarget.coerceAtLeast(23.0) }
                            line.startsWith("G28") -> homed = true
                        }
                    }
                }
                json(buildJsonObject { put("buff", 255) })
            }
            else -> throw Stop(404, "{}")
        }
    }

    private fun model(key: String): JsonElement = synchronized(lock) {
        when (key) {
            "state" -> buildJsonObject {
                put("status", when (state) { "printing" -> "processing"; "paused" -> "paused"; else -> if (disconnected) "halted" else "idle" })
            }
            "job" -> buildJsonObject {
                val active = state == "printing" || state == "paused"
                putJsonObject("file") { put("fileName", if (active) file else null as String?); put("size", size); put("numLayers", 100) }
                put("filePosition", if (active) progress * size else null as Double?)
                put("duration", if (active) printSeconds else null as Double?)
                put("layer", if (active) (progress * 100).toInt() else null as Int?)
                putJsonObject("timesLeft") { put("slicer", if (active) 400 * (1 - progress) else null as Double?) }
            }
            "heat" -> buildJsonObject { put("bedHeaters", buildJsonArray { add(JsonPrimitive(0)) }) }
            "heat.heaters" -> buildJsonArray {
                add(buildJsonObject { put("current", bed); put("active", bedTarget) })
                add(buildJsonObject { put("current", nozzle); put("active", nozzleTarget) })
            }
            "tools" -> buildJsonArray { add(buildJsonObject { put("heaters", buildJsonArray { add(JsonPrimitive(1)) }) }) }
            else -> JsonObject(emptyMap())
        }
    }
}
