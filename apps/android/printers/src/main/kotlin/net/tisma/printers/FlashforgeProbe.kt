package net.tisma.printers

import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress
import java.net.SocketTimeoutException

/**
 * Flashforge's discovery (printer_connections.md, «Flashforge en red local», Descubrimiento): any UDP packet to the
 * multicast group 225.0.0.9 on port 19000 and to broadcast port 48899 (the modern ones), and to the group on port 8899
 * (the older ones); each printer answers the sender with its name, ports and (modern) serial number. Only the local
 * network is asked. Finding never connects: the person adds the printer.
 */
object FlashforgeProbe {
    val TARGETS = listOf(
        InetSocketAddress(Flashforge.MULTICAST, Flashforge.MODERN_MULTICAST_PORT),
        InetSocketAddress("255.255.255.255", Flashforge.MODERN_BROADCAST_PORT),
        InetSocketAddress(Flashforge.MULTICAST, Flashforge.LEGACY_MULTICAST_PORT),
    )

    /** Sends the probe and gathers the answers for [waitMs]; each printer once, by its address. */
    fun probe(targets: List<InetSocketAddress> = TARGETS, waitMs: Int = 2_000): List<FoundPrinter> {
        val found = linkedMapOf<String, FoundPrinter>()
        DatagramSocket().use { s ->
            s.broadcast = true
            val hello = byteArrayOf(0)
            targets.forEach { t -> runCatching { s.send(DatagramPacket(hello, hello.size, t)) } }
            val end = System.currentTimeMillis() + waitMs
            val buf = ByteArray(1024)
            while (true) {
                val left = end - System.currentTimeMillis()
                if (left <= 0) break
                s.soTimeout = left.toInt()
                val p = DatagramPacket(buf, buf.size)
                try { s.receive(p) } catch (_: SocketTimeoutException) { break }
                val a = Flashforge.parseAnswer(p.data.copyOf(p.length)) ?: continue
                val host = p.address.hostAddress ?: continue
                found[host] = FoundPrinter(PrinterKind.FLASHFORGE, a.name.ifBlank { "Flashforge" }, host,
                    if (a.modern) a.httpPort else a.tcpPort, serial = a.serial, model = if (a.modern) "" else "legacy")
            }
        }
        return found.values.toList()
    }
}
