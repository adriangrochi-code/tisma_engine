package net.tisma.app.printers

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import net.tisma.cloud.UrlConnectionHttp
import net.tisma.printers.DiscoveryTypes
import net.tisma.printers.FoundPrinter
import net.tisma.printers.MoonrakerConnection
import net.tisma.printers.PrinterKind
import java.util.ArrayDeque
import java.util.concurrent.Executors

/**
 * Searches the local network with Android's DNS-SD (NsdManager) while the
 * printer list is open (printer_connections.md, «Búsqueda en la red»):
 * `_octoprint._tcp` (its TXT `path`, `model`), `_moonraker._tcp`, and plain
 * `_http._tcp` services, which count as Klipper only if Moonraker's API answers.
 */
class NetworkDiscovery(
    context: Context,
    private val onFound: (FoundPrinter) -> Unit,
    private val onLost: (host: String, port: Int) -> Unit,
) {
    private val nsd = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val listeners = mutableListOf<NsdManager.DiscoveryListener>()
    private val probe = Executors.newSingleThreadExecutor()
    // NsdManager resolves one service at a time on older Androids: a queue.
    private val pending = ArrayDeque<Pair<NsdServiceInfo, String>>()
    private var resolving = false
    private val resolved = mutableMapOf<String, Pair<String, Int>>() // service name -> host, port

    fun start() {
        if (listeners.isNotEmpty()) return
        listOf(DiscoveryTypes.OCTOPRINT, DiscoveryTypes.MOONRAKER, DiscoveryTypes.HTTP).forEach { type ->
            val l = object : NsdManager.DiscoveryListener {
                override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) = Unit
                override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) = Unit
                override fun onDiscoveryStarted(serviceType: String) = Unit
                override fun onDiscoveryStopped(serviceType: String) = Unit
                override fun onServiceFound(info: NsdServiceInfo) = enqueue(info, type)
                override fun onServiceLost(info: NsdServiceInfo) {
                    synchronized(this@NetworkDiscovery) { resolved.remove(type + info.serviceName) }
                        ?.let { (h, p) -> onLost(h, p) }
                }
            }
            runCatching { nsd.discoverServices(type, NsdManager.PROTOCOL_DNS_SD, l) }.onSuccess { listeners += l }
        }
    }

    fun stop() {
        listeners.forEach { runCatching { nsd.stopServiceDiscovery(it) } }
        listeners.clear()
        synchronized(this) { pending.clear() }
    }

    @Synchronized
    private fun enqueue(info: NsdServiceInfo, type: String) {
        pending.add(info to type)
        next()
    }

    @Synchronized
    private fun next() {
        if (resolving) return
        val (info, type) = pending.poll() ?: return
        resolving = true
        @Suppress("DEPRECATION")
        nsd.resolveService(info, object : NsdManager.ResolveListener {
            override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) = done()
            override fun onServiceResolved(s: NsdServiceInfo) {
                @Suppress("DEPRECATION")
                val host = s.host?.hostAddress
                if (host != null) handle(s, type, host)
                done()
            }
        })
    }

    private fun done() {
        synchronized(this) { resolving = false }
        next()
    }

    private fun handle(s: NsdServiceInfo, type: String, host: String) {
        val txt = s.attributes.mapValues { (_, v) -> v?.toString(Charsets.UTF_8).orEmpty() }
        synchronized(this) { resolved[type + s.serviceName] = host to s.port }
        when (type) {
            DiscoveryTypes.OCTOPRINT -> onFound(FoundPrinter(PrinterKind.OCTOPRINT, s.serviceName, host, s.port,
                txt["path"] ?: "/", listOfNotNull(txt["vendor"], txt["model"]).joinToString(" ")))
            DiscoveryTypes.MOONRAKER -> onFound(FoundPrinter(PrinterKind.MOONRAKER, s.serviceName, host, s.port))
            else -> probe.execute {
                // A plain web service is a Klipper printer only if Moonraker answers there.
                val p = FoundPrinter(PrinterKind.MOONRAKER, s.serviceName, host, s.port)
                if (runCatching { MoonrakerConnection(p.url, null, UrlConnectionHttp).test() }.isSuccess) onFound(p)
            }
        }
    }
}
