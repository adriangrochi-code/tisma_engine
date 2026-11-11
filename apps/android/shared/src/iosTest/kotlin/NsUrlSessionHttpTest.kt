import net.tisma.cloud.CloudClient
import net.tisma.cloud.CloudException
import net.tisma.cloud.HttpRequest
import net.tisma.cloud.NsUrlSessionHttp
import net.tisma.io.IOException
import kotlin.test.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** The iPhone's sender (NSURLSession): no answer is an error, never a hang (runs on the iPhone simulator). */
class NsUrlSessionHttpTest {
    @Test
    fun aClosedPortIsANetworkError() {
        assertFailsWith<IOException> { NsUrlSessionHttp.send(HttpRequest("GET", "http://127.0.0.1:9/v1/health", timeoutMs = 5_000)) }
    }

    @Test
    fun notAnAddressIsAnError() {
        assertFailsWith<IOException> { NsUrlSessionHttp.send(HttpRequest("GET", "not a url", timeoutMs = 1_000)) }
    }

    @Test
    fun theCloudClientSaysNetworkWhenTheServerIsDown() {
        val e = assertFailsWith<CloudException> { CloudClient("http://127.0.0.1:9").health() }
        assertTrue(e.network)
    }
}
