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
    /** Gives the part's bytes; it may be called twice (once to measure, when [length] is unknown). */
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
    /** A raw body streamed with its exact length (a file upload by PUT or POST). */
    val stream: FormPart? = null,
) {
    fun withHeader(name: String, value: String) = HttpRequest(method, url, headers + (name to value), body, form, timeoutMs, stream)
}

/** An answer. [body] is read only when the caller asks for it, so a large result can be streamed. */
class HttpResponse(val status: Int, val headers: Map<String, String>, val body: InputStream) : AutoCloseable {
    val ok get() = status in 200..299
    /**
     * The body as text, at most [max] bytes: any device on the network can answer a printer's port (discovery asks
     * many), and an endless answer must not take the phone's memory.
     */
    fun text(max: Int = MAX_TEXT): String = body.use {
        val b = it.readUpTo(max + 1)
        if (b.size > max) throw java.io.IOException("The answer is larger than $max bytes")
        b.toString(Charsets.UTF_8)
    }

    companion object {
        const val MAX_TEXT = 16 * 1024 * 1024
    }
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
                val part = request.stream
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

/** The headers of one part of a multipart/form-data body (RFC 7578). */
private fun partHead(boundary: String, part: FormPart): ByteArray {
    val disposition = buildString {
        append("form-data; name=\"").append(quote(part.name)).append('"')
        part.filename?.let { append("; filename=\"").append(quote(it)).append('"') }
    }
    return "--$boundary\r\nContent-Disposition: $disposition\r\nContent-Type: ${part.contentType}\r\n\r\n"
        .toByteArray(Charsets.UTF_8)
}

private fun closing(boundary: String) = "--$boundary--\r\n".toByteArray()

/**
 * The exact length of a multipart body. A part of unknown length is measured
 * by reading it once (Tisma Cloud refuses uploads without Content-Length:
 * it checks the size before reading, QA AND1).
 */
fun multipartLength(boundary: String, parts: List<FormPart>): Long = parts.sumOf { part ->
    val len = if (part.length >= 0) part.length else part.open().use { measure(it) }
    partHead(boundary, part).size + len + 2
} + closing(boundary).size

/** Writes a multipart/form-data body (RFC 7578). */
fun writeMultipart(out: OutputStream, boundary: String, parts: List<FormPart>) {
    for (part in parts) {
        out.write(partHead(boundary, part))
        part.open().use { it.copyTo(out, 64 * 1024) }
        out.write("\r\n".toByteArray())
    }
    out.write(closing(boundary))
}

// Quotes and line breaks cannot go inside a quoted header value.
private fun quote(s: String) = s.replace("\"", "%22").replace("\r", "").replace("\n", "")

// Android before API 33 lacks the JDK 9 stream helpers (the empty stream, read-n-bytes, read-all):
// these helpers do the same with what every Android has.
fun emptyStream(): InputStream = java.io.ByteArrayInputStream(ByteArray(0))

/** Up to [max] bytes of a stream (fewer at its end). */
fun InputStream.readUpTo(max: Int): ByteArray {
    val out = java.io.ByteArrayOutputStream()
    val buf = ByteArray(minOf(max, 64 * 1024).coerceAtLeast(1))
    var left = max
    while (left > 0) {
        val n = read(buf, 0, minOf(buf.size, left))
        if (n < 0) break
        out.write(buf, 0, n)
        left -= n
    }
    return out.toByteArray()
}

/** How many bytes a stream gives, read to its end. */
fun measure(input: InputStream): Long {
    var n = 0L
    val buf = ByteArray(64 * 1024)
    while (true) {
        val r = input.read(buf)
        if (r < 0) break
        n += r
    }
    return n
}
