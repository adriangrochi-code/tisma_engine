package net.tisma.printers

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import net.tisma.cloud.FormPart
import net.tisma.cloud.Http
import net.tisma.cloud.HttpRequest
import net.tisma.cloud.HttpResponse
import java.io.IOException

/** JSON over HTTP to a printer's program on the local network. */
internal class Rest(
    baseUrl: String,
    private val headers: Map<String, String>,
    private val http: Http,
    /** HTTP Digest credentials (PrusaLink), or null. */
    private val digest: Digest? = null,
) {
    val base = baseUrl.trimEnd('/')
    /** Headers added after connecting (Duet's session key). */
    @Volatile var extraHeaders: Map<String, String> = emptyMap()

    fun get(path: String): JsonElement = call(HttpRequest("GET", base + path, all(), timeoutMs = 10_000))

    fun post(path: String, body: JsonObject? = null): JsonElement {
        val h = if (body != null) all() + ("Content-Type" to "application/json") else all()
        return call(HttpRequest("POST", base + path, h, body?.toString()?.toByteArray() ?: ByteArray(0), timeoutMs = 15_000))
    }

    fun put(path: String): JsonElement = call(HttpRequest("PUT", base + path, all(), ByteArray(0), timeoutMs = 15_000))

    fun delete(path: String): JsonElement = call(HttpRequest("DELETE", base + path, all(), timeoutMs = 15_000))

    fun upload(path: String, parts: List<FormPart>): JsonElement =
        call(HttpRequest("POST", base + path, all(), form = parts, timeoutMs = 30 * 60_000))

    /** A file as the raw body of [method] (PrusaLink's PUT, Duet's rr_upload POST). */
    fun uploadRaw(method: String, path: String, part: FormPart, extra: Map<String, String> = emptyMap()): JsonElement =
        call(HttpRequest(method, base + path, all() + extra, stream = part, timeoutMs = 30 * 60_000))

    private fun all() = headers + extraHeaders + ("Accept" to "application/json")

    /** Sends [r]; with Digest, answers the server's challenge once (RFC 7616). */
    private fun call(r: HttpRequest): JsonElement {
        if (digest == null) return parse(send(r))
        val uri = r.url.substringAfter("://").let { "/" + it.substringAfter('/', "") }
        var req = digest.authorization(r.method, uri)?.let { r.withHeader("Authorization", it) } ?: r
        val first = send(req)
        val challenge = first.header("WWW-Authenticate")
        if (first.status == 401 && challenge != null && digest.learn(challenge)) {
            first.close()
            req = r.withHeader("Authorization", digest.authorization(r.method, uri)!!)
            return parse(send(req))
        }
        return parse(first)
    }

    private fun send(r: HttpRequest): HttpResponse = try {
        http.send(r)
    } catch (e: IOException) {
        throw PrinterException(PrinterException.Kind.NETWORK, e.message ?: "No answer from the printer", e)
    }

    private fun parse(r: HttpResponse): JsonElement {
        val text = try {
            r.text()
        } catch (e: IOException) {
            throw PrinterException(PrinterException.Kind.NETWORK, e.message ?: "Answer interrupted", e)
        }
        if (!r.ok) {
            val kind = when (r.status) {
                401, 403 -> PrinterException.Kind.AUTH
                409 -> PrinterException.Kind.CONFLICT
                else -> PrinterException.Kind.SERVER
            }
            throw PrinterException(kind, errorText(text).ifBlank { "HTTP ${r.status}" })
        }
        if (text.isBlank()) return JsonObject(emptyMap())
        return try {
            Json.parseToJsonElement(text)
        } catch (e: Exception) {
            // OctoPrint answers some commands with plain text.
            JsonPrimitive(text)
        }
    }

    /** The error text of a body: Moonraker's `error.message`, OctoPrint's `error`, or the text itself. */
    private fun errorText(body: String): String = runCatching {
        when (val e = (Json.parseToJsonElement(body) as? JsonObject)?.get("error")) {
            is JsonObject -> e.str("message")
            is JsonPrimitive -> e.contentOrNull.orEmpty()
            else -> ""
        }
    }.getOrNull()?.ifBlank { null } ?: body.take(200).trim()
}

internal fun JsonElement?.obj(): JsonObject? = this as? JsonObject
internal fun JsonObject.o(key: String): JsonObject? = this[key] as? JsonObject
internal fun JsonObject.str(key: String): String = (this[key] as? JsonPrimitive)?.contentOrNull.orEmpty()
internal fun JsonObject.num(key: String): Double? =
    (this[key] as? JsonPrimitive)?.takeIf { !it.isString }?.doubleOrNull
internal fun JsonObject.flag(key: String): Boolean = (this[key] as? JsonPrimitive)?.contentOrNull == "true"

internal fun temperature(o: JsonObject?, actual: String = "actual", target: String = "target"): Temperature? {
    o ?: return null
    val a = o.num(actual) ?: return null
    return Temperature(a, o.num(target) ?: 0.0)
}

/** A number for G-code: no exponent, no trailing zeros. */
internal fun num(v: Double): String = java.math.BigDecimal.valueOf(v).stripTrailingZeros().toPlainString()
