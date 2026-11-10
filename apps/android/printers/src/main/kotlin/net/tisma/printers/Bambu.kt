package net.tisma.printers

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import java.io.InputStream
import java.net.URI
import java.security.MessageDigest
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * A Bambu Lab printer in its local mode (printer_connections.md, «Bambu Lab en
 * red local»; the messages from OpenBambuAPI, community documentation):
 * - MQTT 3.1.1 over TLS on port 8883, user `bblp`, the access code as password;
 *   `pushall` on `device/<serial>/request`, the state on `device/<serial>/report`.
 *   The serial is the certificate's name; the certificate is pinned (rule 4).
 * - Files by implicit FTPS on port 990, packed as `<name>.gcode.3mf` with
 *   `Metadata/plate_1.gcode` and its MD5, started with `project_file`.
 * - Controls with `gcode_line`. The access code is never logged (rule 8).
 *
 * The connection stays open while the app uses it: reports come by themselves,
 * and some models send only what changed (so the state is merged).
 */
class BambuConnection(
    private val host: String,
    private val accessCode: String,
    pinnedSha256: String?,
    private val mqttPort: Int = 8883,
    private val ftpPort: Int = 990,
    private val cameraPort: Int = 6000,
    /** Silence after which the link counts as lost: the printer pushes its state at least this often when asked. */
    private val staleMs: Long = 30_000,
) : PrinterConnection {
    val tls = PinnedTls(pinnedSha256)
    @Volatile private var mqtt: MqttClient? = null
    @Volatile private var serial: String? = null
    private val state = mutableMapOf<String, kotlinx.serialization.json.JsonElement>()
    private val lock = Object()

    /** The serial number and certificate fingerprint seen (for the person to confirm the first time). */
    val seenSerial get() = tls.seenName
    val seenFingerprint get() = tls.seenSha256

    private fun client(): MqttClient {
        mqtt?.takeIf { it.connected }?.let { return it }
        synchronized(lock) {
            mqtt?.takeIf { it.connected }?.let { return it }
            val socket = tls.socket(host, mqttPort)
            val sn = tls.seenName ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "The printer's certificate has no name")
            serial = sn
            val c = MqttClient(socket, "tisma-" + System.nanoTime().toString(36), onMessage = { _, payload -> merge(payload) })
            try {
                c.connect("bblp", accessCode)
            } catch (e: java.io.IOException) {
                throw PrinterException(PrinterException.Kind.NETWORK, e.message ?: "MQTT failed", e)
            }
            c.subscribe("device/$sn/report")
            // A new link starts from the printer's own report, not from what the last one left.
            synchronized(lock) { state.clear() }
            mqtt = c
            request(buildJsonObject {
                put("pushing", buildJsonObject {
                    put("sequence_id", "0"); put("command", "pushall"); put("version", 1); put("push_target", 1)
                })
            })
            return c
        }
    }

    private fun merge(payload: ByteArray) {
        val o = runCatching { Json.parseToJsonElement(payload.toString(Charsets.UTF_8)) as JsonObject }.getOrNull() ?: return
        val print = o["print"] as? JsonObject ?: return
        synchronized(lock) {
            state.putAll(print)
            lock.notifyAll()
        }
    }

    private fun request(o: JsonObject, qos: Int = 0) {
        val c = mqtt ?: client()
        c.publish("device/${serial}/request", o.toString().toByteArray(), qos)
    }

    private fun printCommand(command: String, extra: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}, qos: Int = 1) {
        client()
        request(buildJsonObject { put("print", buildJsonObject { put("sequence_id", "0"); put("command", command); extra() }) }, qos)
    }

    override fun test(): String {
        client()
        waitForState(5_000)
        return "Bambu Lab " + (serial ?: "")
    }

    private fun waitForState(ms: Long) {
        val end = System.currentTimeMillis() + ms
        synchronized(lock) {
            while (!state.containsKey("gcode_state") && System.currentTimeMillis() < end) lock.wait(100)
        }
    }

    override fun status(): PrinterStatus {
        val c = client()
        // Ask for everything again when it has been quiet, and give up on a link that stays silent: a Wi-Fi lost
        // without the socket closing would otherwise show the last state («printing 45 %») for ever.
        val quiet = System.currentTimeMillis() - c.lastReceived
        if (quiet > staleMs / 3) {
            runCatching { request(buildJsonObject { put("pushing", buildJsonObject {
                put("sequence_id", "0"); put("command", "pushall"); put("version", 1); put("push_target", 1) }) }) }
            val asked = System.currentTimeMillis()
            synchronized(lock) { while (c.lastReceived < asked && System.currentTimeMillis() - asked < 3_000) lock.wait(50) }
            if (c.lastReceived < asked && System.currentTimeMillis() - c.lastReceived > staleMs) {
                close()
                synchronized(lock) { state.clear() }
                throw PrinterException(PrinterException.Kind.NETWORK, "The printer stopped answering")
            }
        }
        waitForState(3_000)
        val s = synchronized(lock) { JsonObject(state.toMap()) }
        if (!s.containsKey("gcode_state")) return PrinterStatus(PrinterState.BUSY)
        // Bambu Lab publishes no specification of these values (spec team, 04-11-2026): only IDLE is in the
        // community documentation. RUNNING and PAUSE are taken as printing and paused, to be confirmed on a
        // printer; any other value is shown as it comes, not guessed.
        val raw = s.str("gcode_state")
        val st = when (raw.uppercase()) {
            "RUNNING" -> PrinterState.PRINTING
            "PAUSE" -> PrinterState.PAUSED
            "IDLE" -> PrinterState.READY
            else -> PrinterState.BUSY
        }
        val active = st == PrinterState.PRINTING || st == PrinterState.PAUSED
        val error = s.num("print_error")?.toLong() ?: 0
        return PrinterStatus(
            state = if (error != 0L && active) PrinterState.ERROR else st,
            file = s.str("subtask_name").ifBlank { s.str("gcode_file") },
            percent = if (active) s.num("mc_percent") else null,
            // mc_remaining_time has no documented unit: not shown until checked on a printer.
            remainingS = null,
            layer = s.num("layer_num")?.toInt(),
            totalLayers = s.num("total_layer_num")?.toInt()?.takeIf { it > 0 },
            nozzle = s.num("nozzle_temper")?.let { Temperature(it, s.num("nozzle_target_temper") ?: 0.0) },
            bed = s.num("bed_temper")?.let { Temperature(it, s.num("bed_target_temper") ?: 0.0) },
            chamber = s.num("chamber_temper")?.let { Temperature(it, 0.0) },
            message = when {
                error != 0L -> "Error ${"%08X".format(error)}"
                st == PrinterState.BUSY -> raw
                else -> ""
            },
        )
    }

    override fun pause() = printCommand("pause", { put("param", "") })
    override fun resume() = printCommand("resume", { put("param", "") })
    override fun cancel() = printCommand("stop", { put("param", "") })

    override fun upload(name: String, length: Long, start: Boolean, open: () -> InputStream) =
        upload(name, length, start, PrintOptions(), open)

    override fun upload(name: String, length: Long, start: Boolean, options: PrintOptions, open: () -> InputStream) {
        client()
        val base = name.substringAfterLast('/').removeSuffix(".gcode").removeSuffix(".3mf")
        val remote = "$base.gcode.3mf"
        // Through a temporary file, never whole in memory: a long print's G-code is hundreds of MB.
        val packed = java.io.File.createTempFile("bambu", ".gcode.3mf")
        val gcodeMd5 = try {
            val sum = open().use { pack(it, packed) }
            FtpsUpload(tls, host, ftpPort).store("bblp", accessCode, remote, packed.length()) { packed.inputStream().buffered() }
            sum
        } finally {
            packed.delete()
        }
        if (start) printCommand("project_file", {
            put("param", "Metadata/plate_1.gcode")
            put("project_id", "0"); put("profile_id", "0"); put("task_id", "0"); put("subtask_id", "0")
            put("subtask_name", base)
            put("url", "ftp:///$remote")
            put("md5", gcodeMd5)
            put("timelapse", options.timelapse); put("bed_type", "auto"); put("bed_levelling", options.bedLeveling)
            put("flow_cali", options.flowCalibration); put("vibration_cali", options.vibrationCalibration); put("layer_inspect", false)
            put("ams_mapping", ""); put("use_ams", false)
        })
    }

    override fun camera(): Camera? = null

    /**
     * The A1 and P1 camera (printer_connections.md, Bambu Lab rule 6, after OpenBambuAPI's video page): TLS to
     * port 6000 with the printer's pinned certificate, an 80-byte login (a 16-byte header: size 0x40, type 0x3000,
     * then `bblp` and the access code in 32 bytes each, padded with zeros), then JPEG images, each after a 16-byte
     * header whose first four bytes (little-endian) are its size.
     */
    override fun cameraFrames(): CameraFrames {
        val socket = try {
            tls.socket(host, cameraPort)
        } catch (e: PrinterException) {
            throw e
        } catch (e: java.io.IOException) {
            throw PrinterException(PrinterException.Kind.NETWORK, "The camera does not answer on port $cameraPort", e)
        }
        try {
            socket.soTimeout = 10_000
            socket.getOutputStream().apply { write(cameraLogin(accessCode)); flush() }
        } catch (e: java.io.IOException) {
            socket.close()
            throw PrinterException(PrinterException.Kind.NETWORK, "The camera closed the connection", e)
        }
        val input = java.io.DataInputStream(socket.getInputStream().buffered())
        return object : CameraFrames {
            private var seen = false
            override fun next(): ByteArray {
                val head = ByteArray(16)
                try {
                    input.readFully(head)
                } catch (e: java.io.EOFException) {
                    // Closed before any image: the access code was refused.
                    throw PrinterException(if (seen) PrinterException.Kind.NETWORK else PrinterException.Kind.AUTH,
                        if (seen) "The camera stopped" else "The camera refused the access code", e)
                } catch (e: java.net.SocketTimeoutException) {
                    throw PrinterException(PrinterException.Kind.UNEXPECTED, "The camera sends no images", e)
                }
                val size = le32(head, 0)
                if (size <= 0 || size > 8 * 1024 * 1024) throw PrinterException(PrinterException.Kind.UNEXPECTED, "A camera image of $size bytes")
                val jpeg = ByteArray(size.toInt())
                try { input.readFully(jpeg) } catch (e: java.io.IOException) {
                    throw PrinterException(PrinterException.Kind.NETWORK, "The camera stopped", e)
                }
                val ok = jpeg.size >= 4 && jpeg[0] == 0xFF.toByte() && jpeg[1] == 0xD8.toByte() &&
                    jpeg[jpeg.size - 2] == 0xFF.toByte() && jpeg[jpeg.size - 1] == 0xD9.toByte()
                if (!ok) throw PrinterException(PrinterException.Kind.UNEXPECTED, "A camera image is not a JPEG")
                seen = true
                return jpeg
            }

            override fun close() = runCatching { socket.close() }.let { }
        }
    }

    private fun gcode(lines: String) = printCommand("gcode_line", { put("param", lines + "\n") }, qos = 0)

    override fun setTemperature(heater: Heater, target: Double) =
        gcode(if (heater == Heater.NOZZLE) "M104 S${num(target)}" else "M140 S${num(target)}")

    override fun home(axes: Set<Axis>) = gcode("G28" + axes.sorted().joinToString("") { " " + it.name })

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) =
        gcode("G91\nG1 ${axis.name}${num(distance)} F${num(feedMmS * 60)}\nG90")

    override fun extrude(mm: Double, feedMmS: Double) = gcode("M83\nG1 E${num(mm)} F${num(feedMmS * 60)}")

    override fun close() {
        mqtt?.close()
        mqtt = null
    }

    companion object {
        /** From a saved address, `bambu://host` (tests may give other ports: `bambu://host:mqtt?ftp=port`). */
        fun of(url: String, accessCode: String, pinned: String?, staleMs: Long = 30_000): BambuConnection {
            val u = URI(if (url.startsWith("bambu://")) url else "bambu://$url")
            fun q(k: String) = u.query?.split('&')?.firstOrNull { it.startsWith("$k=") }?.substringAfter('=')?.toIntOrNull()
            return BambuConnection(u.host, accessCode, pinned, if (u.port > 0) u.port else 8883, q("ftp") ?: 990, q("cam") ?: 6000, staleMs)
        }

        /** The camera's 80-byte login: the header, then `bblp` and the access code, 32 bytes each. */
        fun cameraLogin(accessCode: String): ByteArray {
            val b = java.nio.ByteBuffer.allocate(80).order(java.nio.ByteOrder.LITTLE_ENDIAN)
            b.putInt(0x40); b.putInt(0x3000); b.putInt(0); b.putInt(0)
            fun field(v: String) { val raw = v.toByteArray(Charsets.US_ASCII).copyOf(32); b.put(raw) }
            field("bblp"); field(accessCode)
            return b.array()
        }

        private fun le32(b: ByteArray, at: Int): Long =
            (b[at].toLong() and 0xFF) or ((b[at + 1].toLong() and 0xFF) shl 8) or
                ((b[at + 2].toLong() and 0xFF) shl 16) or ((b[at + 3].toLong() and 0xFF) shl 24)

        /** The G-code packed as Bambu Lab's local mode reads it: `Metadata/plate_1.gcode` and its MD5. */
        /**
         * Streams [gcode] into a Bambu project file at [out] (the G-code and its MD5 beside it) and returns that MD5.
         * The G-code is read once, to a temporary copy, so its MD5 is known before the ZIP is written.
         */
        fun pack(gcode: InputStream, out: java.io.File): String {
            val copy = java.io.File.createTempFile("bambu", ".gcode")
            try {
                val md = MessageDigest.getInstance("MD5")
                copy.outputStream().buffered().use { o ->
                    val b = ByteArray(64 * 1024)
                    while (true) { val n = gcode.read(b); if (n < 0) break; md.update(b, 0, n); o.write(b, 0, n) }
                }
                val sum = md.digest().joinToString("") { "%02x".format(it) }
                ZipOutputStream(out.outputStream().buffered()).use { z ->
                    z.putNextEntry(ZipEntry("Metadata/plate_1.gcode")); copy.inputStream().use { it.copyTo(z, 64 * 1024) }; z.closeEntry()
                    z.putNextEntry(ZipEntry("Metadata/plate_1.gcode.md5")); z.write(sum.toByteArray()); z.closeEntry()
                }
                return sum
            } finally {
                copy.delete()
            }
        }

        fun md5(b: ByteArray): String = MessageDigest.getInstance("MD5").digest(b).joinToString("") { "%02x".format(it) }
    }
}
