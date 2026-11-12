package net.tisma.cloud

import net.tisma.io.ByteInput
import net.tisma.io.ByteOutput
import net.tisma.io.BytesInput
import net.tisma.io.BytesOutput
import net.tisma.io.IOException
import net.tisma.io.copyAll

// The HTTP requests and answers of the cloud and printer clients, in common code for Android and iOS. Each platform
// sends them its own way (`UrlConnectionHttp` on Android and the JVM).

private val CRLF = byteArrayOf(13, 10)

/** One part of a multipart/form-data body. */
class FormPart(
    val name: String,
    val contentType: String,
    val filename: String? = null,
    /** The length in bytes when known (for the progress of an upload). */
    val length: Long = -1,
    /** Gives the part's bytes; it may be called twice (once to measure, when [length] is unknown). */
    val open: () -> ByteInput,
) {
    companion object {
        fun text(name: String, value: String, contentType: String = "text/plain; charset=utf-8") =
            value.encodeToByteArray().let { bytes ->
                FormPart(name, contentType, null, bytes.size.toLong()) { BytesInput(bytes) }
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
    /**
     * The answer may be large (a result of up to 1 GB): a platform that would hold it in memory writes it to a file
     * first and reads it from there (iOS). Android streams every answer anyway.
     */
    val large: Boolean = false,
) {
    fun withHeader(name: String, value: String) = HttpRequest(method, url, headers + (name to value), body, form, timeoutMs, stream, large)
}

/** An answer. [body] is read only when the caller asks for it, so a large result can be streamed. */
class HttpResponse(val status: Int, val headers: Map<String, String>, val body: ByteInput) : AutoCloseable {
    val ok get() = status in 200..299
    /**
     * The body as text, at most [max] bytes: any device on the network can answer a printer's port (discovery asks
     * many), and an endless answer must not take the phone's memory.
     */
    fun text(max: Int = MAX_TEXT): String = body.use {
        val b = it.readUpTo(max + 1)
        if (b.size > max) throw IOException("The answer is larger than $max bytes")
        b.decodeToString()
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

/** The headers of one part of a multipart/form-data body (RFC 7578). */
private fun partHead(boundary: String, part: FormPart): ByteArray {
    val disposition = buildString {
        append("form-data; name=\"").append(quote(part.name)).append('"')
        part.filename?.let { append("; filename=\"").append(quote(it)).append('"') }
    }
    return "--$boundary\r\nContent-Disposition: $disposition\r\nContent-Type: ${part.contentType}\r\n\r\n"
        .encodeToByteArray()
}

private fun closing(boundary: String) = "--$boundary--\r\n".encodeToByteArray()

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
fun writeMultipart(out: ByteOutput, boundary: String, parts: List<FormPart>) {
    for (part in parts) {
        partHead(boundary, part).let { out.write(it, 0, it.size) }
        part.open().use { it.copyAll(out) }
        out.write(CRLF, 0, 2)
    }
    closing(boundary).let { out.write(it, 0, it.size) }
}

// Quotes and line breaks cannot go inside a quoted header value.
private fun quote(s: String) = s.replace("\"", "%22").replace("\r", "").replace("\n", "")

// Android before API 33 lacks the JDK 9 stream helpers (the empty stream, read-n-bytes, read-all), and iOS has none:
// these helpers do the same in common code.
fun emptyStream(): ByteInput = BytesInput(ByteArray(0))

/** Up to [max] bytes of a stream (fewer at its end). */
fun ByteInput.readUpTo(max: Int): ByteArray {
    val out = BytesOutput()
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
fun measure(input: ByteInput): Long {
    var n = 0L
    val buf = ByteArray(64 * 1024)
    while (true) {
        val r = input.read(buf, 0, buf.size)
        if (r < 0) break
        n += r
    }
    return n
}

/** How this platform sends requests: `UrlConnectionHttp` on Android and the JVM, NSURLSession on iOS. */
expect val PlatformHttp: Http

/** The app's platform, as the server's User-Agent names it (`Tisma-Android`, `Tisma-iOS`). */
expect val PLATFORM_NAME: String

/**
 * Percent-encodes a path segment or query value (RFC 3986): letters, digits and `-._~` stay, every other UTF-8
 * byte becomes `%XX`, a space included.
 */
fun percentEncode(s: String): String {
    val out = StringBuilder(s.length + 8)
    for (b in s.encodeToByteArray()) {
        val c = b.toInt() and 0xFF
        val ch = c.toChar()
        if (ch in 'A'..'Z' || ch in 'a'..'z' || ch in '0'..'9' || ch == '-' || ch == '.' || ch == '_' || ch == '~') out.append(ch)
        else out.append('%').append(HEX[c shr 4]).append(HEX[c and 15])
    }
    return out.toString()
}

private const val HEX = "0123456789ABCDEF"
