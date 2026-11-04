package net.tisma.cloud

import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.util.UUID

/** One part of a multipart/form-data body. */
class FormPart(
    val name: String,
    val contentType: String,
    val filename: String? = null,
    /** The length in bytes when known (for the progress of an upload). */
    val length: Long = -1,
    val open: () -> InputStream,
) {
    companion object {
        fun text(name: String, value: String, contentType: String = "text/plain; charset=utf-8") =
            value.toByteArray(Charsets.UTF_8).let { bytes ->
                FormPart(name, contentType, null, bytes.size.toLong()) { bytes.inputStream() }
            }
    }
}

class HttpRequest(
    val method: String,
    val url: String,
    val headers: Map<String, String> = emptyMap(),
    val body: ByteArray? = null,
    val form: List<FormPart> = emptyList(),
    val timeoutMs: Int = 30_000,
)

/** An answer. [body] is read only when the caller asks for it, so a large result can be streamed. */
class HttpResponse(val status: Int, val headers: Map<String, String>, val body: InputStream) : AutoCloseable {
    val ok get() = status in 200..299
    fun text(): String = body.use { it.readBytes().toString(Charsets.UTF_8) }
    fun header(name: String): String? = headers.entries.firstOrNull { it.key.equals(name, ignoreCase = true) }?.value
    override fun close() = body.close()
}

/** Sends requests. Tests and the app may put their own in place of [UrlConnectionHttp]. */
fun interface Http {
    /** Throws [IOException] when no answer came. */
    fun send(request: HttpRequest): HttpResponse
}

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
            return HttpResponse(e.responseCode(), emptyMap(), InputStream.nullInputStream())
        } catch (e: IOException) {
            throw written.exceptionOrNull() ?: e
        }
        val headers = c.headerFields.filterKeys { it != null }.mapValues { it.value.joinToString(",") }
        val stream = (if (status >= 400) c.errorStream else c.inputStream) ?: InputStream.nullInputStream()
        return HttpResponse(status, headers, stream)
    }

    private fun writeBody(c: HttpURLConnection, request: HttpRequest) {
        when {
            request.form.isNotEmpty() -> {
                val boundary = "tisma-" + UUID.randomUUID().toString()
                c.setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
                c.doOutput = true
                c.setChunkedStreamingMode(64 * 1024)
                c.outputStream.use { writeMultipart(it, boundary, request.form) }
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

/** Writes a multipart/form-data body (RFC 7578). */
fun writeMultipart(out: OutputStream, boundary: String, parts: List<FormPart>) {
    val crlf = "\r\n"
    for (part in parts) {
        val disposition = buildString {
            append("form-data; name=\"").append(quote(part.name)).append('"')
            part.filename?.let { append("; filename=\"").append(quote(it)).append('"') }
        }
        out.write("--$boundary${crlf}Content-Disposition: $disposition${crlf}Content-Type: ${part.contentType}$crlf$crlf"
            .toByteArray(Charsets.UTF_8))
        part.open().use { it.copyTo(out, 64 * 1024) }
        out.write(crlf.toByteArray())
    }
    out.write("--$boundary--$crlf".toByteArray())
}

// Quotes and line breaks cannot go inside a quoted header value.
private fun quote(s: String) = s.replace("\"", "%22").replace("\r", "").replace("\n", "")
