import net.tisma.printers.PinnedTls
import net.tisma.printers.PrinterException
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

/** The iPhone's TLS link: no answer is a network error within the time given, never a hang. */
class PinnedTlsTest {
    @Test
    fun aClosedPortIsANetworkError() {
        val tls = PinnedTls(null)
        val e = assertFailsWith<PrinterException> { tls.open("127.0.0.1", 9, 3_000) }
        assertEquals(PrinterException.Kind.NETWORK, e.kind)
        assertNull(tls.seenSha256)
    }
}
