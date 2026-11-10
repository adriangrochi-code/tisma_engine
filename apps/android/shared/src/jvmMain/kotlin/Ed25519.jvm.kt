package net.tisma.app.updates

import java.io.InputStream
import java.math.BigInteger
import java.security.MessageDigest

/**
 * Ed25519 signature checking (RFC 8032, section 5.1: the curve, point
 * decoding in 5.1.3, addition in 5.1.4, verification in 5.1.7), for the
 * release list and the packages (app_updates.md, rules 5 and 13). Only
 * verification: the app never signs. Written from the RFC's description, in
 * affine coordinates with BigInteger: slow next to a native one, but it runs
 * a few times per update. Android has no Ed25519 of its own before API 33.
 */
actual object Ed25519 {
    private val TWO: BigInteger = BigInteger.valueOf(2) // BigInteger.TWO is API 33
    private val P: BigInteger = TWO.pow(255) - BigInteger.valueOf(19)
    private val L: BigInteger = TWO.pow(252) + BigInteger("27742317777372353535851937790883648493")
    private val D: BigInteger = BigInteger.valueOf(-121665).mod(P) * BigInteger.valueOf(121666).modInverse(P) % P
    private val SQRT_M1: BigInteger = TWO.modPow((P - BigInteger.ONE) / BigInteger.valueOf(4), P)

    private class Point(val x: BigInteger, val y: BigInteger)

    private val ZERO = Point(BigInteger.ZERO, BigInteger.ONE)
    private val B: Point = run {
        val y = BigInteger.valueOf(4) * BigInteger.valueOf(5).modInverse(P) % P
        decodeXY(y, 0) ?: error("base point")
    }

    /** -x² + y² = 1 + d·x²·y²: the twisted Edwards addition, complete for this curve. */
    private fun add(a: Point, b: Point): Point {
        val t = D * a.x % P * b.x % P * a.y % P * b.y % P
        val x = (a.x * b.y + b.x * a.y) % P * (BigInteger.ONE + t).mod(P).modInverse(P) % P
        val y = (a.y * b.y + a.x * b.x) % P * (BigInteger.ONE - t).mod(P).modInverse(P) % P
        return Point(x, y)
    }

    private fun times(k: BigInteger, p: Point): Point {
        var r = ZERO
        var q = p
        for (i in 0 until k.bitLength()) {
            if (k.testBit(i)) r = add(r, q)
            q = add(q, q)
        }
        return r
    }

    private fun decodeXY(y: BigInteger, sign: Int): Point? {
        if (y >= P) return null
        val y2 = y * y % P
        val u = (y2 - BigInteger.ONE).mod(P)
        val v = (D * y2 + BigInteger.ONE) % P
        // x = u·v³·(u·v⁷)^((p−5)/8), then fixed by √−1 if needed (5.1.3, step 3).
        val v3 = v.modPow(BigInteger.valueOf(3), P)
        val v7 = v.modPow(BigInteger.valueOf(7), P)
        var x = u * v3 % P * (u * v7 % P).modPow((P - BigInteger.valueOf(5)) / BigInteger.valueOf(8), P) % P
        val vx2 = v * x % P * x % P
        when {
            vx2 == u -> {}
            vx2 == (P - u).mod(P) -> x = x * SQRT_M1 % P
            else -> return null
        }
        if (x.signum() == 0 && sign == 1) return null
        if (x.testBit(0) != (sign == 1)) x = P - x
        return Point(x, y)
    }

    /** A 32-byte encoding (little-endian y, the top bit the sign of x) to a point, or null if it is not one. */
    private fun decode(b: ByteArray): Point? {
        if (b.size != 32) return null
        val le = b.copyOf()
        val sign = (le[31].toInt() shr 7) and 1
        le[31] = (le[31].toInt() and 0x7f).toByte()
        return decodeXY(littleEndian(le), sign)
    }

    private fun littleEndian(b: ByteArray) = BigInteger(1, b.reversedArray())

    private fun same(a: Point, b: Point) = a.x == b.x && a.y == b.y

    /** Checks a signature of a message read from [message] (the whole stream) with a 32-byte public key. */
    fun verify(publicKey: ByteArray, message: InputStream, signature: ByteArray): Boolean {
        if (publicKey.size != 32 || signature.size != 64) return false
        val a = decode(publicKey) ?: return false
        val rBytes = signature.copyOfRange(0, 32)
        val r = decode(rBytes) ?: return false
        val s = littleEndian(signature.copyOfRange(32, 64))
        if (s >= L) return false
        val h = MessageDigest.getInstance("SHA-512")
        h.update(rBytes); h.update(publicKey)
        val buf = ByteArray(64 * 1024)
        while (true) {
            val n = message.read(buf)
            if (n < 0) break
            h.update(buf, 0, n)
        }
        val k = littleEndian(h.digest()).mod(L)
        // [8][S]B = [8]R + [8][k]A (5.1.7, step 3).
        val eight = BigInteger.valueOf(8)
        val left = times(eight, times(s, B))
        val right = times(eight, add(r, times(k, a)))
        return same(left, right)
    }

    actual fun verify(publicKey: ByteArray, message: ByteArray, signature: ByteArray): Boolean =
        verify(publicKey, message.inputStream(), signature)
}
