package net.tisma.mockcloud

import com.sun.net.httpserver.HttpServer
import java.net.InetSocketAddress

/**
 * Serves fixed files over HTTP on this machine, for tests that download (the
 * profile pack, the updates). `Range: bytes=N-` gives the rest with 206, as a
 * server that lets a cut download go on (RFC 9110, 14.2).
 */
class StaticFiles(private val files: Map<String, ByteArray>, port: Int = 0) : AutoCloseable {
    /** How many requests each path got. */
    val hits = java.util.concurrent.ConcurrentHashMap<String, Int>()

    /** The `Range` headers received, in order. */
    val ranges = java.util.concurrent.CopyOnWriteArrayList<String>()

    private val server = HttpServer.create(InetSocketAddress("127.0.0.1", port), 0).apply {
        createContext("/") { ex ->
            val path = ex.requestURI.path.trimStart('/')
            hits.merge(path, 1, Int::plus)
            val body = files[path]
            ex.requestHeaders.getFirst("Range")?.let { ranges += it }
            val from = ex.requestHeaders.getFirst("Range")?.let { Regex("""bytes=(\d+)-""").find(it)?.groupValues?.get(1)?.toIntOrNull() }
            when {
                body == null -> { ex.sendResponseHeaders(404, -1); ex.close() }
                from != null && from >= body.size -> { ex.sendResponseHeaders(416, -1); ex.close() }
                from != null -> {
                    ex.responseHeaders.add("Content-Range", "bytes $from-${body.size - 1}/${body.size}")
                    ex.sendResponseHeaders(206, (body.size - from).toLong())
                    ex.responseBody.use { it.write(body, from, body.size - from) }
                }
                else -> { ex.sendResponseHeaders(200, body.size.toLong()); ex.responseBody.use { it.write(body) } }
            }
        }
        start()
    }

    val url: String get() = "http://127.0.0.1:${server.address.port}"

    override fun close() = server.stop(0)
}
