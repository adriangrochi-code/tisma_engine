package net.tisma.mockcloud

import com.sun.net.httpserver.HttpServer
import java.net.InetSocketAddress

/** Serves fixed files over HTTP on this machine, for tests that download (the profile pack). */
class StaticFiles(private val files: Map<String, ByteArray>) : AutoCloseable {
    private val server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0).apply {
        createContext("/") { ex ->
            val body = files[ex.requestURI.path.trimStart('/')]
            if (body == null) { ex.sendResponseHeaders(404, -1); ex.close() }
            else { ex.sendResponseHeaders(200, body.size.toLong()); ex.responseBody.use { it.write(body) } }
        }
        start()
    }

    val url: String get() = "http://127.0.0.1:${server.address.port}"

    override fun close() = server.stop(0)
}
