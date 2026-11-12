package net.tisma.io

/**
 * MD5 (RFC 1321, §3), for HTTP Digest authentication (RFC 7616) and Bambu's file check, where the protocol asks for
 * it. Not for security of our own: the app uses SHA-256 everywhere it chooses. Own code from the RFC.
 */
object Md5 {
    private val S = intArrayOf(7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21)

    // T[i] = floor(2^32 × |sin(i + 1)|), RFC 1321 §3.4.
    private val T = IntArray(64) { i -> (kotlin.math.abs(kotlin.math.sin((i + 1).toDouble())) * 4294967296.0).toLong().toInt() }

    fun digest(input: ByteArray): ByteArray = Md5Digest().apply { update(input, 0, input.size) }.digest()

    internal fun block(st: IntArray, m: ByteArray, at: Int) {
        val x = IntArray(16)
        for (j in 0 until 16) {
            val o = at + j * 4
            x[j] = (m[o].toInt() and 0xFF) or ((m[o + 1].toInt() and 0xFF) shl 8) or
                ((m[o + 2].toInt() and 0xFF) shl 16) or ((m[o + 3].toInt() and 0xFF) shl 24)
        }
        var a = st[0]; var b = st[1]; var c = st[2]; var d = st[3]
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
        st[0] += a; st[1] += b; st[2] += c; st[3] += d
    }
}

/** MD5 over data that comes in parts (a G-code of hundreds of MB), RFC 1321 §3. */
class Md5Digest {
    private val st = intArrayOf(0x67452301, 0xefcdab89.toInt(), 0x98badcfe.toInt(), 0x10325476)
    private val buf = ByteArray(64)
    private var n = 0
    private var total = 0L

    fun update(b: ByteArray, off: Int, len: Int) {
        total += len
        var i = off
        val end = off + len
        while (i < end) {
            val k = minOf(64 - n, end - i)
            b.copyInto(buf, n, i, i + k)
            n += k; i += k
            if (n == 64) { Md5.block(st, buf, 0); n = 0 }
        }
    }

    fun digest(): ByteArray {
        val bits = total * 8
        val pad = ByteArray(if (n < 56) 56 - n else 120 - n).also { it[0] = 0x80.toByte() }
        update(pad, 0, pad.size)
        val len = ByteArray(8) { i -> (bits ushr (8 * i)).toByte() }
        update(len, 0, 8)
        return ByteArray(16) { i -> (st[i / 4] ushr (8 * (i % 4))).toByte() }
    }
}

/** CRC-32 (ISO-HDLC, the polynomial 0xEDB88320 reflected) as ZIP uses it (PKWARE APPNOTE, 4.4.7). */
class Crc32 {
    private var c = 0xFFFFFFFF.toInt()
    fun update(b: ByteArray, off: Int, len: Int) {
        for (i in off until off + len) c = TABLE[(c xor b[i].toInt()) and 0xFF] xor (c ushr 8)
    }
    val value: Long get() = (c.inv().toLong() and 0xFFFFFFFFL)

    private companion object {
        val TABLE = IntArray(256) { n ->
            var c = n
            repeat(8) { c = if (c and 1 != 0) 0xEDB88320.toInt() xor (c ushr 1) else c ushr 1 }
            c
        }
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
