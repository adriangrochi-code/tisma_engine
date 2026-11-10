package net.tisma.printers

import net.tisma.cloud.Http
import net.tisma.cloud.HttpRequest
import net.tisma.cloud.UrlConnectionHttp
import net.tisma.cloud.readUpTo
import java.io.IOException

/**
 * One still image from a printer's camera (printer_connections.md, «Cámara de
 * la impresora»): its snapshot address, read once each time the screen asks.
 * Images stay in memory only (rule 4 of «Cámara USB»: they are not kept).
 */
object Snapshot {
    const val MAX_BYTES = 8 * 1024 * 1024

    fun fetch(url: String, http: Http = UrlConnectionHttp): ByteArray {
        if (!url.startsWith("http://") && !url.startsWith("https://")) {
            throw PrinterException(PrinterException.Kind.UNEXPECTED, "Not an http address: $url")
        }
        val r = try {
            http.send(HttpRequest("GET", url, mapOf("Accept" to "image/jpeg,image/*"), timeoutMs = 5_000))
        } catch (e: IOException) {
            throw PrinterException(PrinterException.Kind.NETWORK, e.message ?: "The camera does not answer", e)
        }
        r.use {
            if (!it.ok) throw PrinterException(PrinterException.Kind.SERVER, "HTTP ${it.status}")
            val bytes = it.body.readUpTo(MAX_BYTES + 1)
            if (bytes.size > MAX_BYTES) throw PrinterException(PrinterException.Kind.UNEXPECTED, "The image is too large")
            return bytes
        }
    }
}
