package net.tisma.printers

import net.tisma.io.ByteInput
import net.tisma.io.ByteOutput
import net.tisma.io.BytesOutput
import net.tisma.io.IOException
import net.tisma.io.Link
import net.tisma.io.Lock
import net.tisma.io.nowMs
import net.tisma.io.readInto
import net.tisma.io.startDaemon
import kotlin.concurrent.Volatile

/**
 * A small MQTT 3.1.1 client (OASIS Standard, 29 October 2014), written from the
 * specification for what Bambu Lab's local mode needs (printer_connections.md,
 * «Bambu Lab en red local»): CONNECT with user and password, SUBSCRIBE, PUBLISH
 * at QoS 0 and 1, PINGREQ and DISCONNECT. The socket (TLS for the printer) is
 * opened by the caller (a [Link]: TLS for the printer). Incoming messages go to [onMessage] from a reader thread.
 * Common code, for Android and iOS.
 */
class MqttClient(
    private val socket: Link,
    private val clientId: String,
    private val onMessage: (topic: String, payload: ByteArray) -> Unit,
    private val onClosed: (Throwable?) -> Unit = {},
) {
    private val input: ByteInput = socket.input
    private val output: ByteOutput = socket.output
    private val lock = Lock()
    private var ids = 0
    @Volatile var connected = false
        private set
    /** When the last packet came in (ms). A link lost without a close (Wi-Fi gone) shows here as silence. */
    @Volatile var lastReceived: Long = nowMs()
        private set

    /** Section 3.1: CONNECT; waits for CONNACK. Throws [PrinterException] AUTH on a refused login. */
    fun connect(user: String?, password: String?, keepAliveS: Int = 60) {
        val body = Bytes()
        body.string("MQTT")
        body.byte(4) // protocol level 3.1.1
        var flags = 0x02 // clean session
        if (user != null) flags = flags or 0x80
        if (password != null) flags = flags or 0x40
        body.byte(flags)
        body.short(keepAliveS)
        body.string(clientId)
        user?.let { body.string(it) }
        password?.let { body.string(it) }
        send(0x10, body.toByteArray())
        val (type, payload) = readPacket()
        if (type shr 4 != 2 || payload.size < 2) throw IOException("No CONNACK")
        when (payload[1].toInt()) {
            0 -> Unit
            4, 5 -> throw PrinterException(PrinterException.Kind.AUTH, "The printer refused the access code")
            else -> throw IOException("CONNACK refused: ${payload[1]}")
        }
        connected = true
        startDaemon("mqtt-reader") { readLoop() }
    }

    /** Section 3.8: SUBSCRIBE at QoS 0 (the SUBACK is read by the reader thread). */
    fun subscribe(topic: String) {
        val body = Bytes()
        body.short(nextId())
        body.string(topic)
        body.byte(0)
        send(0x82, body.toByteArray())
    }

    /** Section 3.3: PUBLISH at QoS 0 or 1 (the PUBACK is read by the reader thread). */
    fun publish(topic: String, payload: ByteArray, qos: Int = 0) {
        val body = Bytes()
        body.string(topic)
        if (qos > 0) body.short(nextId())
        body.raw(payload)
        send(0x30 or (qos.coerceIn(0, 1) shl 1), body.toByteArray())
    }

    fun ping() = send(0xC0, ByteArray(0))

    fun close() {
        runCatching { send(0xE0, ByteArray(0)) }
        connected = false
        runCatching { socket.close() }
    }

    private fun nextId(): Int = lock.locked {
        ids = (ids + 1) and 0xFFFF
        if (ids == 0) ids = 1
        ids
    }

    private fun readLoop() {
        var error: Throwable? = null
        try {
            while (connected) {
                val (type, body) = readPacket()
                when (type shr 4) {
                    3 -> { // PUBLISH
                        val qos = (type shr 1) and 3
                        val tlen = ((body[0].toInt() and 0xFF) shl 8) or (body[1].toInt() and 0xFF)
                        val topic = body.decodeToString(2, 2 + tlen)
                        var pos = 2 + tlen
                        if (qos > 0) {
                            val id = ((body[pos].toInt() and 0xFF) shl 8) or (body[pos + 1].toInt() and 0xFF)
                            pos += 2
                            send(0x40, byteArrayOf((id shr 8).toByte(), id.toByte())) // PUBACK
                        }
                        onMessage(topic, body.copyOfRange(pos, body.size))
                    }
                    else -> Unit // SUBACK, PUBACK, PINGRESP
                }
            }
        } catch (e: Throwable) {
            if (connected) error = e
        } finally {
            connected = false
            runCatching { socket.close() }
            onClosed(error)
        }
    }

    private fun send(header: Int, body: ByteArray) = lock.locked {
        val out = Bytes()
        out.byte(header)
        out.varint(body.size)
        out.raw(body)
        val b = out.toByteArray()
        output.write(b, 0, b.size)
        output.flush()
    }

    /** One packet: its first byte and its body (section 2.2: the remaining length as a varint). */
    private fun readPacket(): Pair<Int, ByteArray> {
        val first = input.read()
        if (first < 0) throw IOException("The printer closed the link")
        var mult = 1
        var len = 0
        while (true) {
            val b = input.read()
            if (b < 0) throw IOException("The printer closed the link")
            len += (b and 0x7F) * mult
            if (b and 0x80 == 0) break
            mult *= 128
            if (mult > 128 * 128 * 128) throw IOException("Bad remaining length")
        }
        // A printer's report is a few tens of KB: a bigger length is a broken or hostile peer, not a reason to take
        // hundreds of MB of the phone's memory.
        if (len > MAX_PACKET) throw IOException("MQTT packet too big: $len bytes")
        val body = ByteArray(len)
        if (input.readInto(body) < len) throw IOException("The printer closed the link")
        lastReceived = nowMs()
        return first to body
    }

    /** A byte buffer with MQTT's encodings. */
    private class Bytes {
        private val out = BytesOutput()
        fun byte(b: Int) = out.write(b and 0xFF)
        fun short(v: Int) { out.write((v shr 8) and 0xFF); out.write(v and 0xFF) }
        fun raw(b: ByteArray) = out.write(b, 0, b.size)
        fun string(s: String) { val b = s.encodeToByteArray(); short(b.size); raw(b) }
        fun varint(v: Int) {
            var x = v
            do {
                var d = x % 128
                x /= 128
                if (x > 0) d = d or 0x80
                out.write(d)
            } while (x > 0)
        }
        fun toByteArray(): ByteArray = out.toByteArray()
    }

    companion object {
        const val MAX_PACKET = 16 * 1024 * 1024

        /** Reads one MQTT packet from a stream (for the simulator's broker). */
        fun readFrom(input: ByteInput): Pair<Int, ByteArray>? {
            val first = input.read()
            if (first < 0) return null
            var mult = 1
            var len = 0
            while (true) {
                val b = input.read()
                if (b < 0) return null
                len += (b and 0x7F) * mult
                if (b and 0x80 == 0) break
                mult *= 128
            }
            val body = ByteArray(len)
            if (input.readInto(body) < len) return null
            return first to body
        }

        /** Writes one MQTT packet (for the simulator's broker). */
        fun writeTo(out: ByteOutput, header: Int, body: ByteArray) {
            val b = Bytes()
            b.byte(header); b.varint(body.size); b.raw(body)
            val bytes = b.toByteArray()
            writeLock.locked { out.write(bytes, 0, bytes.size); out.flush() }
        }

        private val writeLock = Lock()
    }
}
