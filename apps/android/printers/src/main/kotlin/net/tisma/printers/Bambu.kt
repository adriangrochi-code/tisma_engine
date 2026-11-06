package net.tisma.printers

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import java.io.ByteArrayOutputStream
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
        client()
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
        val gcode = open().use { it.readBytes() }
        val base = name.substringAfterLast('/').removeSuffix(".gcode").removeSuffix(".3mf")
        val packed = pack(gcode)
        val remote = "$base.gcode.3mf"
        FtpsUpload(tls, host, ftpPort).store("bblp", accessCode, remote, packed.size.toLong()) { packed.inputStream() }
        if (start) printCommand("project_file", {
            put("param", "Metadata/plate_1.gcode")
            put("project_id", "0"); put("profile_id", "0"); put("task_id", "0"); put("subtask_id", "0")
            put("subtask_name", base)
            put("url", "ftp:///$remote")
            put("md5", md5(gcode))
            put("timelapse", options.timelapse); put("bed_type", "auto"); put("bed_levelling", options.bedLeveling)
            put("flow_cali", options.flowCalibration); put("vibration_cali", options.vibrationCalibration); put("layer_inspect", false)
            put("ams_mapping", ""); put("use_ams", false)
        })
    }

    override fun camera(): Camera? = null

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
        fun of(url: String, accessCode: String, pinned: String?): BambuConnection {
            val u = URI(if (url.startsWith("bambu://")) url else "bambu://$url")
            val ftp = u.query?.split('&')?.firstOrNull { it.startsWith("ftp=") }?.substringAfter('=')?.toIntOrNull() ?: 990
            return BambuConnection(u.host, accessCode, pinned, if (u.port > 0) u.port else 8883, ftp)
        }

        /** The G-code packed as Bambu Lab's local mode reads it: `Metadata/plate_1.gcode` and its MD5. */
        fun pack(gcode: ByteArray): ByteArray {
            val out = ByteArrayOutputStream()
            ZipOutputStream(out).use { z ->
                z.putNextEntry(ZipEntry("Metadata/plate_1.gcode")); z.write(gcode); z.closeEntry()
                z.putNextEntry(ZipEntry("Metadata/plate_1.gcode.md5")); z.write(md5(gcode).toByteArray()); z.closeEntry()
            }
            return out.toByteArray()
        }

        fun md5(b: ByteArray): String = MessageDigest.getInstance("MD5").digest(b).joinToString("") { "%02x".format(it) }
    }
}
