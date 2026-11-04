package net.tisma.printers

import java.security.MessageDigest
import java.security.SecureRandom

/**
 * HTTP Digest access authentication, RFC 7616 (MD5, qop "auth"), as PrusaLink
 * asks for it (its OpenAPI: `scheme: digest`). Own code from the RFC. The
 * password never goes on the wire: only the hashes do.
 */
class Digest(private val user: String, private val password: String) {
    private var realm: String? = null
    private var nonce: String? = null
    private var opaque: String? = null
    private var qop: String? = null
    private var nc = 0
    private val random = SecureRandom()

    /** Reads a `WWW-Authenticate: Digest …` challenge; false if it is not a Digest one. */
    @Synchronized
    fun learn(header: String): Boolean {
        if (!header.trim().startsWith("Digest", ignoreCase = true)) return false
        val p = params(header.trim().substring(6))
        realm = p["realm"] ?: return false
        nonce = p["nonce"] ?: return false
        opaque = p["opaque"]
        qop = p["qop"]?.split(',')?.map { it.trim() }?.firstOrNull { it == "auth" }
        nc = 0
        return true
    }

    /** The `Authorization` header for a request, once a challenge is known. */
    @Synchronized
    fun authorization(method: String, uri: String): String? {
        val r = realm ?: return null
        val n = nonce ?: return null
        val ha1 = md5("$user:$r:$password")
        val ha2 = md5("$method:$uri")
        val sb = StringBuilder("Digest username=\"").append(user).append("\", realm=\"").append(r)
            .append("\", nonce=\"").append(n).append("\", uri=\"").append(uri).append('"')
        if (qop != null) {
            nc++
            val ncText = "%08x".format(nc)
            val cnonce = ByteArray(8).also { random.nextBytes(it) }.joinToString("") { "%02x".format(it) }
            val response = md5("$ha1:$n:$ncText:$cnonce:$qop:$ha2")
            sb.append(", qop=").append(qop).append(", nc=").append(ncText).append(", cnonce=\"").append(cnonce)
                .append("\", response=\"").append(response).append('"')
        } else {
            sb.append(", response=\"").append(md5("$ha1:$n:$ha2")).append('"')
        }
        opaque?.let { sb.append(", opaque=\"").append(it).append('"') }
        sb.append(", algorithm=MD5")
        return sb.toString()
    }

    companion object {
        fun md5(s: String): String =
            MessageDigest.getInstance("MD5").digest(s.toByteArray(Charsets.UTF_8)).joinToString("") { "%02x".format(it) }

        /** `key="value", key=value` pairs of an auth header. */
        fun params(text: String): Map<String, String> {
            val out = mutableMapOf<String, String>()
            Regex("""(\w+)\s*=\s*(?:"([^"]*)"|([^\s,]+))""").findAll(text).forEach { m ->
                out[m.groupValues[1].lowercase()] = m.groupValues[2].ifEmpty { m.groupValues[3] }
            }
            return out
        }
    }
}
