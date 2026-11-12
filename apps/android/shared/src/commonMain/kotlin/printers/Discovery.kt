package net.tisma.printers

/**
 * Printers found on the local network (printer_connections.md, «Búsqueda en
 * la red»). Finding never connects: the person adds the printer. Only the
 * local network is searched; nothing goes out (rule 7).
 */
data class FoundPrinter(
    val kind: PrinterKind,
    /** The name in the announcement. */
    val name: String,
    val host: String,
    val port: Int,
    /** OctoPrint's `path` from its TXT record (rule of the table). */
    val path: String = "/",
    /** OctoPrint's `model` and `vendor`, when announced. */
    val model: String = "",
) {
    /** The address the printer is added with: the service's address plus `path`. */
    val url: String
        get() {
            val h = if (host.contains(':') && !host.startsWith("[")) "[$host]" else host
            val p = path.trim().ifEmpty { "/" }.let { if (it.startsWith("/")) it else "/$it" }.trimEnd('/')
            return "http://$h:$port$p"
        }

    /** One printer once, by its address and port (rule 2). */
    val key: String get() = "${host.lowercase()}:$port"
}

/** The list the menu shows: each printer once, gone after 3 minutes without news (rules 2 and 3). */
class FoundPrinters(private val forgetAfterMs: Long = 3 * 60_000L) {
    private val seen = linkedMapOf<String, Pair<FoundPrinter, Long>>()
    private val lock = net.tisma.io.Lock()

    /** Adds or refreshes a printer. OctoPrint's own service wins over a plain web one at the same address. */
    fun seen(p: FoundPrinter, now: Long) = lock.locked {
        val old = seen[p.key]?.first
        val keep = if (old != null && old.kind == PrinterKind.OCTOPRINT && p.kind != PrinterKind.OCTOPRINT) old else p
        seen[p.key] = keep to now
    }

    fun lost(host: String, port: Int) = lock.locked {
        seen.remove("${host.lowercase()}:$port")
        Unit
    }

    /** The printers heard from in the last 3 minutes, without the ones already saved (by address). */
    fun current(now: Long, saved: Collection<String> = emptyList()): List<FoundPrinter> = lock.locked {
        val old = seen.entries.filter { now - it.value.second > forgetAfterMs }.map { it.key }
        old.forEach { seen.remove(it) }
        val savedUrls = saved.map { it.trimEnd('/').lowercase() }.toSet()
        seen.values.map { it.first }.filter { it.url.lowercase() !in savedUrls }
    }
}

/** DNS-SD types searched (the table of «Búsqueda en la red»). */
object DiscoveryTypes {
    const val OCTOPRINT = "_octoprint._tcp"
    const val MOONRAKER = "_moonraker._tcp"
    /** Plain web services: a Moonraker is told apart by asking its API. */
    const val HTTP = "_http._tcp"
}
