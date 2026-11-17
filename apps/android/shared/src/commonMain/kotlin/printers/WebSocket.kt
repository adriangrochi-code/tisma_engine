package net.tisma.printers

import kotlin.io.encoding.Base64
import net.tisma.io.BytesOutput
import net.tisma.io.IOException
import net.tisma.io.Link
import net.tisma.io.Lock
import net.tisma.io.Sha1
import net.tisma.io.secureRandomBytes

/**
 * A WebSocket client (RFC 6455) over a plain TCP [Link], for printers on the local network (Elegoo's SDCP on port
 * 3030, Creality's state on 9999). Text messages only, as those printers speak; frames from the client are masked
 * (§5.3), pings are answered with pongs (§5.5.2), and a close ends [receive] with null. Own code from the RFC.
 */
class WebSocket private constructor(private val link: Link) : AutoCloseable {
    private val sendLock = Lock()

    /** The handshake was refused: the server's HTTP status and its first words (Elegoo: 500 «too many client»). */
    class Refused(val status: Int, val body: String) : IOException("WebSocket refused: HTTP $status $body")

    /** Sends one text message. */
    fun send(text: String) = frame(0x1, text.encodeToByteArray())

    /** The next text message, or null when the server closed. Control frames are handled on the way. */
    fun receive(): String? {
        val message = BytesOutput()
        while (true) {
            val b0 = read()
            val b1 = read()
            val fin = b0 and 0x80 != 0
            val opcode = b0 and 0x0F
            var len = (b1 and 0x7F).toLong()
            if (len == 126L) len = ((read() shl 8) or read()).toLong()
            else if (len == 127L) { len = 0; repeat(8) { len = (len shl 8) or read().toLong() } }
            if (len > MAX) throw IOException("A WebSocket message of $len bytes")
            val mask = if (b1 and 0x80 != 0) ByteArray(4) { read().toByte() } else null
            val payload = ByteArray(len.toInt())
            var got = 0
            while (got < payload.size) {
                val n = link.input.read(payload, got, payload.size - got)
                if (n < 0) throw IOException("The WebSocket was cut")
                got += n
            }
            if (mask != null) for (i in payload.indices) payload[i] = (payload[i].toInt() xor mask[i % 4].toInt()).toByte()
            when (opcode) {
                0x8 -> { runCatching { frame(0x8, ByteArray(0)) }; return null }
                0x9 -> frame(0xA, payload)
                0xA -> Unit
                0x0, 0x1, 0x2 -> {
                    message.write(payload, 0, payload.size)
                    if (fin) return message.toByteArray().decodeToString()
                }
                else -> throw IOException("Unknown WebSocket opcode $opcode")
            }
        }
    }

    private fun read(): Int {
        val b = link.input.read()
        if (b < 0) throw IOException("The WebSocket was cut")
        return b
    }

    private fun frame(opcode: Int, payload: ByteArray) = sendLock.locked {
        val head = BytesOutput()
        head.write(0x80 or opcode)
        when {
            payload.size < 126 -> head.write(0x80 or payload.size)
            payload.size < 65536 -> { head.write(0x80 or 126); head.write(payload.size shr 8); head.write(payload.size and 0xFF) }
            else -> { head.write(0x80 or 127); for (i in 7 downTo 0) head.write(((payload.size.toLong() ushr (8 * i)) and 0xFF).toInt()) }
        }
        val mask = secureRandomBytes(4)
        head.write(mask, 0, 4)
        val masked = ByteArray(payload.size) { (payload[it].toInt() xor mask[it % 4].toInt()).toByte() }
        val h = head.toByteArray()
        link.output.write(h, 0, h.size)
        link.output.write(masked, 0, masked.size)
        link.output.flush()
    }

    override fun close() {
        runCatching { frame(0x8, ByteArray(0)) }
        runCatching { link.close() }
    }

    companion object {
        /** The largest message accepted (a printer's state is a few kB). */
        const val MAX = 4L shl 20
        private const val GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

        /** Opens `ws://host:port/path` (RFC 6455, §4.1): the upgrade request, and the server's 101 with the right accept key. */
        fun connect(host: String, port: Int, path: String, timeoutMs: Int = 10_000, readTimeoutMs: Int = 30_000): WebSocket {
            // [readTimeoutMs] holds for the handshake and every later read: the caller keeps the link talking (pings).
            val link = openTcp(host, port, timeoutMs, readTimeoutMs)
            try {
                val key = Base64.encode(secureRandomBytes(16))
                val request = "GET $path HTTP/1.1\r\nHost: $host:$port\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                    "Sec-WebSocket-Key: $key\r\nSec-WebSocket-Version: 13\r\n\r\n"
                val r = request.encodeToByteArray()
                link.output.write(r, 0, r.size)
                link.output.flush()
                val head = StringBuilder()
                while (!head.endsWith("\r\n\r\n")) {
                    val b = link.input.read()
                    if (b < 0) break
                    head.append(b.toChar())
                    if (head.length > 16_384) throw IOException("The WebSocket answer is too long")
                }
                val lines = head.toString().split("\r\n")
                val status = lines.firstOrNull()?.split(' ')?.getOrNull(1)?.toIntOrNull() ?: 0
                if (status != 101) {
                    // The body, if any, says why (Elegoo: «too many client»).
                    val body = BytesOutput()
                    runCatching { repeat(512) { val b = link.input.read(); if (b < 0) return@runCatching; body.write(b) } }
                    throw Refused(status, body.toByteArray().decodeToString().trim())
                }
                val accept = lines.firstOrNull { it.startsWith("Sec-WebSocket-Accept:", ignoreCase = true) }?.substringAfter(':')?.trim()
                val want = Base64.encode(Sha1.digest((key + GUID).encodeToByteArray()))
                if (accept != want) throw IOException("The WebSocket answer has a wrong accept key")
                return WebSocket(link)
            } catch (e: Throwable) {
                runCatching { link.close() }
                throw e
            }
        }
    }
}
