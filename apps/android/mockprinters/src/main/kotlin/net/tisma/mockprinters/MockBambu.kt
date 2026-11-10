package net.tisma.mockprinters

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.put
import net.tisma.printers.MqttClient
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader
import java.io.OutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.security.KeyStore
import java.util.zip.ZipInputStream
import javax.net.ssl.KeyManagerFactory
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLServerSocket
import javax.net.ssl.SSLSocket

/**
 * A Bambu Lab printer in local mode, for the tests (printer_connections.md,
 * BL1 to BL3): an MQTT 3.1.1 broker over TLS that answers `pushall` and the
 * print commands with reports, and an implicit FTPS server that keeps what is
 * stored. Its certificate is made with the JDK's keytool, named by the serial.
 */
class MockBambu(val serial: String = "01S00C123456789", val accessCode: String = "12345678") {
    private val dir = File(System.getProperty("java.io.tmpdir"), "mockbambu-" + System.nanoTime()).apply { mkdirs() }
    private val ssl: SSLContext = run {
        val ks = File(dir, "printer.p12")
        val keytool = File(System.getProperty("java.home"), "bin/keytool").path
        val p = ProcessBuilder(keytool, "-genkeypair", "-alias", "printer", "-keyalg", "RSA", "-keysize", "2048",
            "-dname", "CN=$serial", "-validity", "30", "-storetype", "PKCS12", "-keystore", ks.path,
            "-storepass", "changeit", "-keypass", "changeit").redirectErrorStream(true).start()
        p.inputStream.readBytes()
        check(p.waitFor() == 0) { "keytool failed" }
        val store = KeyStore.getInstance("PKCS12").apply { ks.inputStream().use { load(it, "changeit".toCharArray()) } }
        val kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm()).apply { init(store, "changeit".toCharArray()) }
        SSLContext.getInstance("TLS").apply { init(kmf.keyManagers, null, null) }
    }
    private val loopback = InetAddress.getByName("127.0.0.1")
    private val mqttServer = ssl.serverSocketFactory.createServerSocket(0, 10, loopback) as SSLServerSocket
    private val ftpServer = ssl.serverSocketFactory.createServerSocket(0, 10, loopback) as SSLServerSocket
    private val cameraServer = ssl.serverSocketFactory.createServerSocket(0, 10, loopback) as SSLServerSocket

    val mqttPort get() = mqttServer.localPort
    val ftpPort get() = ftpServer.localPort
    val cameraPort get() = cameraServer.localPort
    val url get() = "bambu://127.0.0.1:$mqttPort?ftp=$ftpPort&cam=$cameraPort"

    /** The camera's images: a tiny JPEG-shaped frame, numbered, every [cameraMs]. */
    var cameraMs = 50L
    /** The camera logins received (user, access code). */
    val cameraLogins = java.util.concurrent.CopyOnWriteArrayList<Pair<String, String>>()

    var gcodeState = "IDLE"
    var percent = 0
    var nozzle = 25.0
    var nozzleTarget = 0.0
    /** Files stored by FTPS, and the print commands received. */
    val files = java.util.concurrent.ConcurrentHashMap<String, ByteArray>()
    val commands = java.util.concurrent.CopyOnWriteArrayList<JsonObject>()
    private val clients = java.util.concurrent.CopyOnWriteArrayList<OutputStream>()
    @Volatile private var running = true

    fun start(): MockBambu {
        Thread({ acceptLoop(mqttServer) { mqtt(it) } }, "bambu-mqtt").apply { isDaemon = true }.start()
        Thread({ acceptLoop(ftpServer) { ftp(it) } }, "bambu-ftp").apply { isDaemon = true }.start()
        Thread({ acceptLoop(cameraServer) { camera(it) } }, "bambu-camera").apply { isDaemon = true }.start()
        return this
    }

    fun stop() {
        running = false
        runCatching { mqttServer.close() }
        runCatching { ftpServer.close() }
        runCatching { cameraServer.close() }
        dir.deleteRecursively()
    }

    private fun acceptLoop(server: ServerSocket, handle: (Socket) -> Unit) {
        while (running) {
            val s = runCatching { server.accept() }.getOrNull() ?: break
            Thread({ runCatching { s.use { handle(it) } } }, "bambu-conn").apply { isDaemon = true }.start()
        }
    }

    /** The A1/P1 camera as OpenBambuAPI's video page describes it: an 80-byte login, then framed JPEG images. */
    private fun camera(s: Socket) {
        val input = java.io.DataInputStream(s.getInputStream())
        val login = ByteArray(80)
        input.readFully(login)
        fun field(at: Int) = String(login, at, 32, Charsets.US_ASCII).trimEnd('\u0000')
        val user = field(16); val pass = field(48)
        cameraLogins += user to pass
        val head = java.nio.ByteBuffer.wrap(login, 0, 8).order(java.nio.ByteOrder.LITTLE_ENDIAN)
        if (head.int != 0x40 || head.int != 0x3000 || user != "bblp" || pass != accessCode) return // closes
        val out = s.getOutputStream()
        var n = 0
        while (running) {
            val jpeg = byteArrayOf(0xFF.toByte(), 0xD8.toByte()) + "frame ${n++}".toByteArray() + byteArrayOf(0xFF.toByte(), 0xD9.toByte())
            val h = java.nio.ByteBuffer.allocate(16).order(java.nio.ByteOrder.LITTLE_ENDIAN).putInt(jpeg.size).putInt(0).putInt(1).putInt(0)
            out.write(h.array()); out.write(jpeg); out.flush()
            Thread.sleep(cameraMs)
        }
    }

    private fun report(): ByteArray = synchronized(this) {
        buildJsonObject {
            put("print", buildJsonObject {
                put("command", "push_status")
                put("gcode_state", gcodeState); put("mc_percent", percent); put("mc_remaining_time", 10)
                put("nozzle_temper", nozzle); put("nozzle_target_temper", nozzleTarget)
                put("bed_temper", 25.0); put("bed_target_temper", 0.0)
                put("layer_num", percent); put("total_layer_num", 100)
                put("subtask_name", "cube"); put("print_error", 0)
            })
        }.toString().toByteArray()
    }

    private fun push() {
        val r = report()
        synchronized(clients) {
            clients.forEach { out -> runCatching { MqttClient.writeTo(out, 0x30, publishBody("device/$serial/report", r)) } }
        }
    }

    private fun publishBody(topic: String, payload: ByteArray): ByteArray {
        val t = topic.toByteArray()
        return byteArrayOf((t.size shr 8).toByte(), t.size.toByte()) + t + payload
    }

    private fun mqtt(s: Socket) {
        val input = s.getInputStream().buffered()
        val out = s.getOutputStream()
        // CONNECT: check user bblp and the access code.
        val (_, connect) = MqttClient.readFrom(input) ?: return
        var pos = 2 + 4 + 1 + 1 + 2 // "MQTT", level, flags, keep-alive
        fun str(): String {
            val n = ((connect[pos].toInt() and 0xFF) shl 8) or (connect[pos + 1].toInt() and 0xFF)
            val v = String(connect, pos + 2, n); pos += 2 + n; return v
        }
        str() // client id
        val user = str()
        val pass = str()
        val ok = user == "bblp" && pass == accessCode
        MqttClient.writeTo(out, 0x20, byteArrayOf(0, if (ok) 0 else 5))
        if (!ok) return
        synchronized(clients) { clients += out }
        try {
            while (running) {
                val (type, body) = MqttClient.readFrom(input) ?: break
                when (type shr 4) {
                    8 -> MqttClient.writeTo(out, 0x90, byteArrayOf(body[0], body[1], 0)) // SUBACK
                    12 -> MqttClient.writeTo(out, 0xD0, ByteArray(0)) // PINGRESP
                    3 -> {
                        val qos = (type shr 1) and 3
                        val tlen = ((body[0].toInt() and 0xFF) shl 8) or (body[1].toInt() and 0xFF)
                        var p = 2 + tlen
                        if (qos > 0) {
                            MqttClient.writeTo(out, 0x40, byteArrayOf(body[p], body[p + 1])); p += 2
                        }
                        handle(Json.parseToJsonElement(String(body, p, body.size - p)) as JsonObject)
                    }
                    14 -> break
                }
            }
        } finally {
            synchronized(clients) { clients -= out }
        }
    }

    private fun handle(o: JsonObject) {
        (o["print"] as? JsonObject)?.let { p ->
            synchronized(this) {
                commands += p
                when ((p["command"] as? JsonPrimitive)?.contentOrNull) {
                    "project_file" -> { gcodeState = "RUNNING"; percent = 10; nozzleTarget = 220.0; nozzle = 220.0 }
                    "pause" -> if (gcodeState == "RUNNING") gcodeState = "PAUSE"
                    "resume" -> if (gcodeState == "PAUSE") gcodeState = "RUNNING"
                    "stop" -> { gcodeState = "IDLE"; percent = 0; nozzleTarget = 0.0 }
                    "gcode_line" -> (p["param"] as? JsonPrimitive)?.contentOrNull?.lines()?.forEach { line ->
                        if (line.startsWith("M104")) { nozzleTarget = line.substringAfter("S").toDouble(); nozzle = nozzleTarget }
                    }
                }
            }
        }
        push()
    }

    /** Refuse the next upload after opening its data connection (a full card), and whether the phone closed it. */
    @Volatile var refuseStore = false
    @Volatile var refusedDataClosed: Boolean? = null

    private fun ftp(s: Socket) {
        val r = BufferedReader(InputStreamReader(s.getInputStream()))
        val w = s.getOutputStream()
        fun say(t: String) { w.write((t + "\r\n").toByteArray()); w.flush() }
        say("220 Bambu FTPS")
        var user = ""
        var passive: ServerSocket? = null
        while (running) {
            val line = r.readLine() ?: break
            val cmd = line.substringBefore(' ').uppercase()
            val arg = line.substringAfter(' ', "")
            when (cmd) {
                "USER" -> { user = arg; say("331 Password required") }
                "PASS" -> if (user == "bblp" && arg == accessCode) say("230 Logged in") else { say("530 Login incorrect"); return }
                "PBSZ" -> say("200 PBSZ=0")
                "PROT" -> say("200 Protection set to Private")
                "TYPE" -> say("200 Type set to I")
                "PASV" -> {
                    // A plain listening socket; TLS starts on the accepted data connection.
                    passive = ServerSocket(0, 1, loopback)
                    val port = passive.localPort
                    say("227 Entering Passive Mode (127,0,0,1,${port / 256},${port % 256})")
                }
                "STOR" -> {
                    val ps = passive ?: run { say("425 Use PASV first"); null } ?: continue
                    if (refuseStore) {
                        // A full card: the data connection is taken, then the store refused; is it closed by the phone?
                        val raw = ps.accept()
                        val data = ssl.socketFactory.createSocket(raw, raw.inetAddress.hostAddress, raw.port, true) as SSLSocket
                        data.useClientMode = false
                        data.startHandshake()
                        say("552 No space left")
                        data.soTimeout = 5_000
                        refusedDataClosed = runCatching { data.use { it.getInputStream().read() < 0 } }.getOrDefault(false)
                        ps.close()
                        continue
                    }
                    say("150 Opening data connection")
                    val raw = ps.accept()
                    val data = ssl.socketFactory.createSocket(raw, raw.inetAddress.hostAddress, raw.port, true) as SSLSocket
                    data.useClientMode = false
                    val bytes = data.use { it.getInputStream().readBytes() }
                    ps.close()
                    synchronized(this) { files[arg.trimStart('/')] = bytes }
                    say("226 Transfer complete")
                }
                "QUIT" -> { say("221 Bye"); return }
                else -> say("502 Not implemented")
            }
        }
    }

    /** The G-code inside a stored `.gcode.3mf`, and the MD5 written next to it. */
    fun storedGcode(name: String): Pair<ByteArray, String>? {
        val zip = files[name] ?: return null
        var gcode: ByteArray? = null
        var md5 = ""
        ZipInputStream(zip.inputStream()).use { z ->
            generateSequence { z.nextEntry }.forEach { e ->
                when (e.name) {
                    "Metadata/plate_1.gcode" -> gcode = z.readBytes()
                    "Metadata/plate_1.gcode.md5" -> md5 = String(z.readBytes())
                }
            }
        }
        return gcode?.let { it to md5 }
    }
}
