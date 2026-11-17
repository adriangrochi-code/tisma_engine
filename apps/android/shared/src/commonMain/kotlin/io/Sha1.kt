package net.tisma.io

/**
 * SHA-1 (FIPS 180-4, §6.1), only for the WebSocket handshake's `Sec-WebSocket-Accept` (RFC 6455, §4.2.2), where the
 * standard asks for it; never for anything that must resist collisions. Own code from the standard.
 */
object Sha1 {
    fun digest(input: ByteArray): ByteArray {
        var h0 = 0x67452301; var h1 = 0xEFCDAB89.toInt(); var h2 = 0x98BADCFE.toInt(); var h3 = 0x10325476; var h4 = 0xC3D2E1F0.toInt()
        val bits = input.size.toLong() * 8
        val padded = ByteArray(((input.size + 9 + 63) / 64) * 64)
        input.copyInto(padded)
        padded[input.size] = 0x80.toByte()
        for (i in 0 until 8) padded[padded.size - 1 - i] = (bits ushr (8 * i)).toByte()
        val w = IntArray(80)
        for (chunk in padded.indices step 64) {
            for (t in 0 until 16) {
                val o = chunk + 4 * t
                w[t] = ((padded[o].toInt() and 0xFF) shl 24) or ((padded[o + 1].toInt() and 0xFF) shl 16) or
                    ((padded[o + 2].toInt() and 0xFF) shl 8) or (padded[o + 3].toInt() and 0xFF)
            }
            for (t in 16 until 80) w[t] = (w[t - 3] xor w[t - 8] xor w[t - 14] xor w[t - 16]).rotateLeft(1)
            var a = h0; var b = h1; var c = h2; var d = h3; var e = h4
            for (t in 0 until 80) {
                val (f, k) = when {
                    t < 20 -> ((b and c) or (b.inv() and d)) to 0x5A827999
                    t < 40 -> (b xor c xor d) to 0x6ED9EBA1
                    t < 60 -> ((b and c) or (b and d) or (c and d)) to 0x8F1BBCDC.toInt()
                    else -> (b xor c xor d) to 0xCA62C1D6.toInt()
                }
                val temp = a.rotateLeft(5) + f + e + k + w[t]
                e = d; d = c; c = b.rotateLeft(30); b = a; a = temp
            }
            h0 += a; h1 += b; h2 += c; h3 += d; h4 += e
        }
        val out = ByteArray(20)
        intArrayOf(h0, h1, h2, h3, h4).forEachIndexed { i, h -> for (j in 0 until 4) out[4 * i + j] = (h ushr (24 - 8 * j)).toByte() }
        return out
    }
}
