package net.tisma.mockprinters

import java.io.DataInputStream
import java.io.OutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicInteger

/**
 * A Creality printer with its stock firmware for the tests (printer_connections.md, «Creality de fábrica», CR1): the
 * WebSocket on `/` that pushes JSON state by itself, and nothing else (no Moonraker). Any other request is counted and
 * answered 404, as port 80 does on that firmware. Written from the spec.
 */
class MockCreality(port: Int = 0) {
    private val server = ServerSocket(port, 10, InetAddress.getByName("127.0.0.1"))
    val port get() = server.localPort
    val url get() = "creality://127.0.0.1?ws=$port&moonraker=${closedPort()}"
    /** Requests that were not the WebSocket (an upload would be one). */
    val otherRequests = AtomicInteger()
    val clients = AtomicInteger()
    private val outs = CopyOnWriteArrayList<OutputStream>()
    /** What a new client gets first: the state as it is. */
    @Volatile var last = """{"model":"","modelVersion":"F012","hostname":"K1C-1234","printJobTime":0,"printLeftTime":0}"""

    fun start(): MockCreality {
        Thread({
            while (true) {
                val s = runCatching { server.accept() }.getOrNull() ?: break
                Thread({ runCatching { s.use { serve(it) } } }, "creality-conn").apply { isDaemon = true }.start()
            }
        }, "creality-accept").apply { isDaemon = true }.start()
        return this
    }

    fun stop() = runCatching { server.close(); outs.forEach { it.close() } }

    /** Pushes [json] to every client, as the printer does on its own. */
    fun push(json: String) { last = json; outs.forEach { runCatching { frame(it, json) } } }

    /** Cuts every client (a Wi-Fi drop). */
    fun cut() = outs.forEach { runCatching { it.close() } }

    private fun serve(s: java.net.Socket) {
        val input = DataInputStream(s.getInputStream())
        val head = StringBuilder()
        while (!head.endsWith("\r\n\r\n")) { val b = input.read(); if (b < 0) return; head.append(b.toChar()) }
        val lines = head.toString().split("\r\n")
        val headers = lines.drop(1).filter { ':' in it }.associate { it.substringBefore(':').trim().lowercase() to it.substringAfter(':').trim() }
        val out = s.getOutputStream()
        if (!headers["upgrade"].equals("websocket", true)) {
            otherRequests.incrementAndGet()
            out.write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n".toByteArray()); out.flush(); return
        }
        val accept = java.util.Base64.getEncoder().encodeToString(java.security.MessageDigest.getInstance("SHA-1")
            .digest((headers["sec-websocket-key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").toByteArray()))
        out.write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: $accept\r\n\r\n".toByteArray())
        out.flush()
        clients.incrementAndGet(); outs += out
        try {
            frame(out, last)
            while (input.read() >= 0) { /* the client sends nothing the printer acts on */ }
        } finally { outs -= out; clients.decrementAndGet() }
    }

    private fun frame(out: OutputStream, text: String) = synchronized(out) {
        val p = text.toByteArray()
        out.write(0x81)
        if (p.size < 126) out.write(p.size) else { out.write(126); out.write(p.size shr 8); out.write(p.size and 0xFF) }
        out.write(p); out.flush()
    }

    private fun closedPort() = ServerSocket(0).use { it.localPort }
}
