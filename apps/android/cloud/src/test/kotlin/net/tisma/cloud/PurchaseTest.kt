package net.tisma.cloud

import net.tisma.mockcloud.MockCloud
import org.junit.After
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

/** cloud.md 5c, CC21 and CC22: a store purchase verified by the server gives the plan to one account only. */
class PurchaseTest {
    private val server = MockCloud().start()

    @After
    fun stop() = server.stop()

    private fun signed(email: String) = CloudClient(server.url).apply { login(email, "password1", "t") }

    @Test
    fun `CC21 CC22 a confirmed purchase gives the plan, once per account`() {
        val pro = CloudClient(server.url).plans().first { it.name == "Pro" }
        assertEquals("pro_month", pro.storeProducts["google_play"])
        val ana = signed("ana@example.com")
        assertNull(ana.account().subscription)
        val buy = StorePurchase("google_play", "pro_month", purchaseToken = "ok-token-1")
        val a = ana.purchase(buy)
        assertEquals("Pro", a.plan)
        assertEquals(Subscription("google_play", "pro_month", "2026-12-11T00:00:00Z", true), a.subscription)
        // The same purchase again: the same answer.
        assertEquals(a, ana.purchase(buy))
        // From another account: refused.
        val e = assertFailsWith<CloudException> { signed("free@example.com").purchase(buy) }
        assertEquals(409, e.status)
        // Not confirmed by the store, or a field missing.
        assertEquals(402, assertFailsWith<CloudException> { ana.purchase(buy.copy(purchaseToken = "bad")) }.status)
        assertEquals(422, assertFailsWith<CloudException> { ana.purchase(buy.copy(purchaseToken = "")) }.status)
    }
}
