package net.tisma.printers

import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress
import java.net.SocketTimeoutException

/**
 * Elegoo's discovery (printer_connections.md, «Impresoras Elegoo por red local», §2): the text `M99999` by UDP
 * broadcast to port 3000; each printer answers with a JSON. Answers are gathered for a while and kept once per
 * `MainboardID`. Only the local network is asked.
 */
object ElegooProbe {
    val TARGETS = listOf(InetSocketAddress("255.255.255.255", ElegooDiscovery.PORT))

    fun probe(targets: List<InetSocketAddress> = TARGETS, waitMs: Int = 2_000): List<FoundPrinter> {
        val found = linkedMapOf<String, FoundPrinter>()
        DatagramSocket().use { s ->
            s.broadcast = true
            val hello = ElegooDiscovery.PROBE.toByteArray()
            targets.forEach { t -> runCatching { s.send(DatagramPacket(hello, hello.size, t)) } }
            val end = System.currentTimeMillis() + waitMs
            val buf = ByteArray(4096)
            while (true) {
                val left = end - System.currentTimeMillis()
                // In slices, so a search that is stopped (its thread interrupted) ends at once: a receive ignores interrupts.
                if (left <= 0 || Thread.currentThread().isInterrupted) break
                s.soTimeout = minOf(left, 100L).toInt()
                val p = DatagramPacket(buf, buf.size)
                try { s.receive(p) } catch (_: SocketTimeoutException) { continue }
                val a = ElegooDiscovery.parse(String(p.data, 0, p.length, Charsets.UTF_8)) ?: continue
                val key = a.mainboardId.ifBlank { a.ip }
                found[key] = FoundPrinter(PrinterKind.ELEGOO, a.name.ifBlank { "Elegoo" }, a.ip, ElegooConnection.PORT,
                    model = a.model, serial = a.mainboardId)
            }
        }
        return found.values.toList()
    }
}
