import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import net.tisma.cloud.Account
import net.tisma.cloud.CloudException
import net.tisma.cloud.Plan
import net.tisma.cloud.ServiceStatus
import kotlin.test.assertFalse
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The cloud's answers read the same on Android and iPhone (cloud.md; runs on the JVM and the iPhone simulator). */
class CloudModelsTest {
    private fun obj(s: String) = Json.parseToJsonElement(s).jsonObject

    @Test
    fun accountWithAStoreSubscriptionAndMissingFields() {
        val a = Account.from(obj("""{"email":"ana@example.com","plan":"pro","paid":true,"hours_left":"x",
            "subscription":{"store":"app_store","product_id":"pro_month","auto_renew":true},
            "purchase_account_token":"6f1c"}"""))
        assertEquals("ana@example.com", a.email)
        assertEquals(0.0, a.hoursLeft)
        assertTrue(a.hasPassword)
        assertEquals("app_store", a.subscription?.store)
        assertEquals("6f1c", a.purchaseAccountToken)
        assertNull(Account.from(obj("""{"subscription":null}""")).subscription)
    }

    @Test
    fun planStoreProductsSkipEmptyOnes() {
        val p = Plan.from(obj("""{"name":"Pro","classes":["slice"],"store_products":{"google_play":"pro_month","app_store":""}}"""))
        assertEquals(mapOf("google_play" to "pro_month"), p.storeProducts)
        assertTrue(p.includes("slice"))
    }

    @Test
    fun errorTextFromTheServer() {
        assertEquals("Too big", CloudException.detailOf("""{"detail":"Too big"}""", 413))
        assertEquals("bad field", CloudException.detailOf("""{"detail":[{"msg":"bad field"}]}""", 422))
        assertEquals("HTTP 500", CloudException.detailOf("<html>", 500))
    }

    @Test
    fun serviceStatusInTheRightLanguage() {
        val st = ServiceStatus.from(obj("""{"status":"SLOW","label_es":"Con demoras","label_en":"Delayed",
            "message_es":"La cola espera.","message_en":"","unavailable":["aero"],"longest_wait_s":900,"nodes_online":1}"""))
        assertTrue(st.worthNotice)
        assertEquals("Con demoras", st.label(spanish = true))
        assertEquals("Delayed", st.label(spanish = false))
        assertEquals("La cola espera.", st.message(spanish = false), "falls back to the other language")
        assertEquals(listOf("aero"), st.unavailable)
        assertFalse(ServiceStatus.from(obj("""{"status":"ok"}""")).worthNotice)
        assertFalse(ServiceStatus.from(obj("""{}""")).worthNotice)
    }
}
