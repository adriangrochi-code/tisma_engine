import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.runTest
import net.tisma.cloud.AccountModel
import net.tisma.cloud.CloudClient
import net.tisma.cloud.Http
import net.tisma.cloud.HttpResponse
import net.tisma.cloud.TokenStore
import net.tisma.io.BytesInput
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The common account model against a scripted server (JVM and iPhone simulator). */
class AccountModelTest {
    private class Tokens(var t: String? = null) : TokenStore {
        override fun read() = t
        override fun write(token: String?) { t = token }
    }

    private var meStatus = 200

    private val http = Http { r ->
        val path = r.url.substringAfter("https://cloud.test")
        val (code, body) = when {
            path == "/v1/status" -> 200 to """{"status":"down","label_en":"Down","message_en":"No node."}"""
            path == "/v1/plans" -> 200 to """{"plans":[{"name":"Free"},{"name":"Pro"}]}"""
            path == "/v1/auth/login" -> 200 to """{"token":"t1","email":"ana@example.com"}"""
            path == "/v1/auth/logout" -> 200 to """{"ok":true}"""
            path == "/v1/me" -> if (meStatus == 200) 200 to """{"email":"ana@example.com","plan":"Pro","hours_left":7}"""
                else meStatus to """{"detail":"Session expired"}"""
            else -> 404 to """{"detail":"Not Found"}"""
        }
        HttpResponse(code, emptyMap(), BytesInput(body.encodeToByteArray()))
    }

    @Test
    fun signInRefreshAndSignOut() = runTest {
        val tokens = Tokens()
        val m = AccountModel("https://cloud.test", tokens, "test", StandardTestDispatcher(testScheduler)) { s, t -> CloudClient(s, t, http) }
        m.refresh().join()
        assertEquals("down", m.state.value.service?.status)
        assertEquals(listOf("Free", "Pro"), m.state.value.plans.map { it.name })
        assertFalse(m.state.value.signedIn)
        m.signIn("ana@example.com ", "pw").join()
        assertTrue(m.state.value.signedIn)
        assertEquals("t1", tokens.t)
        assertEquals(7.0, m.state.value.account?.hoursLeft)
        assertFalse(m.state.value.busy)
        m.signOut().join()
        assertFalse(m.state.value.signedIn)
        assertNull(m.state.value.account)
        assertNull(tokens.t)
        assertEquals(2, m.state.value.plans.size, "the public plans stay")
        m.close()
    }

    @Test
    fun anExpiredSessionSignsOutAndSaysSo() = runTest {
        val tokens = Tokens("old")
        meStatus = 401
        val m = AccountModel("https://cloud.test", tokens, "test", StandardTestDispatcher(testScheduler)) { s, t -> CloudClient(s, t, http) }
        assertTrue(m.state.value.signedIn)
        m.refresh().join()
        assertFalse(m.state.value.signedIn)
        assertNull(tokens.t)
        assertEquals(401, m.state.value.problem?.status)
        m.close()
    }
}
