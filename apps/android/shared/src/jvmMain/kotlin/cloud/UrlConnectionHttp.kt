package net.tisma.cloud

import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.UUID

/** HTTP and HTTPS over the platform's own client (java.net), which Android and the JVM both have. */
object UrlConnectionHttp : Http {
    override fun send(request: HttpRequest): HttpResponse {
        val c = URL(request.url).openConnection() as HttpURLConnection
        c.requestMethod = request.method
        c.connectTimeout = 10_000
        c.readTimeout = request.timeoutMs
        c.instanceFollowRedirects = false
        c.useCaches = false
        request.headers.forEach { (k, v) -> c.setRequestProperty(k, v) }
        // A server may answer (413) before the whole body is sent and close the
        // connection: the answer is still read when there is one.
        val written = runCatching { writeBody(c, request) }
        written.exceptionOrNull()?.let { e -> if (e !is IOException) throw e }
        val status = try {
            c.responseCode
        } catch (e: java.net.HttpRetryException) {
            // In streaming mode the JVM answers a 401 by throwing instead of returning it.
            return HttpResponse(e.responseCode(), emptyMap(), emptyStream())
        } catch (e: IOException) {
            throw written.exceptionOrNull() ?: e
        }
        val headers = c.headerFields.filterKeys { it != null }.mapValues { it.value.joinToString(",") }
        val stream = (if (status >= 400) c.errorStream else c.inputStream) ?: emptyStream()
        return HttpResponse(status, headers, stream)
    }

    private fun writeBody(c: HttpURLConnection, request: HttpRequest) {
        when {
            request.form.isNotEmpty() -> {
                val boundary = "tisma-" + UUID.randomUUID().toString()
                c.setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
                c.doOutput = true
                // A fixed length, never chunked: the server wants Content-Length before it reads.
                c.setFixedLengthStreamingMode(multipartLength(boundary, request.form))
                c.outputStream.use { writeMultipart(it, boundary, request.form) }
            }
            request.stream != null -> {
                val part = request.stream!! // another module's property: no smart cast
                val length = if (part.length >= 0) part.length else part.open().use { measure(it) }
                c.setRequestProperty("Content-Type", part.contentType)
                c.doOutput = true
                c.setFixedLengthStreamingMode(length)
                c.outputStream.use { out -> part.open().use { it.copyTo(out, 64 * 1024) } }
            }
            request.body != null -> {
                // Small JSON bodies are not streamed, so a 401 keeps its text
                // (in streaming mode the JVM throws it away).
                c.doOutput = true
                c.outputStream.use { it.write(request.body) }
            }
        }
    }
}


/** Android and the JVM send with [UrlConnectionHttp]. */
actual val PlatformHttp: Http get() = UrlConnectionHttp

actual val PLATFORM_NAME: String = "Android"
