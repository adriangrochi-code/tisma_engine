import net.tisma.io.Md5
import net.tisma.io.hex
import net.tisma.printers.Digest
import net.tisma.printers.num
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** The printer clients' common pieces, on the JVM and the iPhone simulator. */
class PrinterCommonTest {
    @Test
    fun md5MatchesTheRfcTestSuite() {
        // RFC 1321, appendix A.5.
        assertEquals("d41d8cd98f00b204e9800998ecf8427e", hex(Md5.digest(ByteArray(0))))
        assertEquals("0cc175b9c0f1b6a831c399e269772661", hex(Md5.digest("a".encodeToByteArray())))
        assertEquals("900150983cd24fb0d6963f7d28e17f72", hex(Md5.digest("abc".encodeToByteArray())))
        assertEquals("c3fcd3d76192e4007dfb496cca67e13b", hex(Md5.digest("abcdefghijklmnopqrstuvwxyz".encodeToByteArray())))
        assertEquals("57edf4a22be3c955ac49da2e2107b67a",
            hex(Md5.digest("12345678901234567890123456789012345678901234567890123456789012345678901234567890".encodeToByteArray())))
    }

    @Test
    fun digestAuthorizationFollowsRfc7616() {
        // RFC 7616 §3.9.1 uses SHA-256; RFC 2617 §3.5 gives the MD5 example: Mufasa, testrealm@host.com.
        val d = Digest("Mufasa", "Circle Of Life")
        assertTrue(d.learn("""Digest realm="testrealm@host.com", qop="auth,auth-int", nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093", opaque="5ccc069c403ebaf9f0171e9517f40e41""""))
        val h = d.authorization("GET", "/dir/index.html")!!
        assertTrue("nc=00000001" in h)
        val cnonce = Regex("cnonce=\"([0-9a-f]{16})\"").find(h)!!.groupValues[1]
        val ha1 = Digest.md5("Mufasa:testrealm@host.com:Circle Of Life")
        val ha2 = Digest.md5("GET:/dir/index.html")
        assertTrue("response=\"" + Digest.md5("$ha1:dcd98b7102dd2f0e8b11d0f600bfb0c093:00000001:$cnonce:auth:$ha2") + "\"" in h)
        assertEquals("939e7578ed9e3c518a452acee763bce9", ha1)
    }

    @Test
    fun numbersArePlainWithAPoint() {
        assertEquals("0.1", num(0.1))
        assertEquals("-12", num(-12.0))
        assertEquals("0.0001", num(0.0001))
        assertEquals("1.5", num(1.50))
        assertEquals("12000000", num(1.2e7))
        assertEquals("0", num(-0.0))
        assertEquals("210", num(210.0))
    }
}
