package net.tisma.io

/**
 * MD5 (RFC 1321, §3), for HTTP Digest authentication (RFC 7616) and Bambu's file check, where the protocol asks for
 * it. Not for security of our own: the app uses SHA-256 everywhere it chooses. Own code from the RFC.
 */
object Md5 {
    private val S = intArrayOf(7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21)

    // T[i] = floor(2^32 × |sin(i + 1)|), RFC 1321 §3.4.
    private val T = IntArray(64) { i -> (kotlin.math.abs(kotlin.math.sin((i + 1).toDouble())) * 4294967296.0).toLong().toInt() }

    fun digest(input: ByteArray): ByteArray {
        val bitLen = input.size.toLong() * 8
        val padded = ((input.size + 8) / 64 + 1) * 64
        val m = input.copyOf(padded)
        m[input.size] = 0x80.toByte()
        for (i in 0 until 8) m[padded - 8 + i] = (bitLen ushr (8 * i)).toByte()
        var a0 = 0x67452301; var b0 = 0xefcdab89.toInt(); var c0 = 0x98badcfe.toInt(); var d0 = 0x10325476
        val x = IntArray(16)
        for (block in 0 until padded / 64) {
            for (j in 0 until 16) {
                val o = block * 64 + j * 4
                x[j] = (m[o].toInt() and 0xFF) or ((m[o + 1].toInt() and 0xFF) shl 8) or
                    ((m[o + 2].toInt() and 0xFF) shl 16) or ((m[o + 3].toInt() and 0xFF) shl 24)
            }
            var a = a0; var b = b0; var c = c0; var d = d0
            for (i in 0 until 64) {
                val f: Int; val g: Int
                when (i / 16) {
                    0 -> { f = (b and c) or (b.inv() and d); g = i }
                    1 -> { f = (d and b) or (d.inv() and c); g = (5 * i + 1) % 16 }
                    2 -> { f = b xor c xor d; g = (3 * i + 5) % 16 }
                    else -> { f = c xor (b or d.inv()); g = (7 * i) % 16 }
                }
                val t = d; d = c; c = b
                b += (a + f + T[i] + x[g]).rotateLeft(S[(i / 16) * 4 + i % 4])
                a = t
            }
            a0 += a; b0 += b; c0 += c; d0 += d
        }
        val out = ByteArray(16)
        for ((k, v) in listOf(a0, b0, c0, d0).withIndex()) for (i in 0 until 4) out[4 * k + i] = (v ushr (8 * i)).toByte()
        return out
    }
}

/** Lower-case hexadecimal of [b]. */
fun hex(b: ByteArray): String {
    val digits = "0123456789abcdef"
    val sb = StringBuilder(b.size * 2)
    for (x in b) { val v = x.toInt() and 0xFF; sb.append(digits[v shr 4]).append(digits[v and 15]) }
    return sb.toString()
}

/** [n] bytes from the platform's secure random source. */
expect fun secureRandomBytes(n: Int): ByteArray
