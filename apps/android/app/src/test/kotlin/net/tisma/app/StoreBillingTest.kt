package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.cloud.StoreBilling
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Screen
import net.tisma.app.ui.Section
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import net.tisma.cloud.StorePurchase
import net.tisma.mockcloud.MockCloud
import org.junit.After
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import kotlin.test.assertEquals

/** cloud.md 5c: a plan bought in the phone's store and verified by the server; purchases restored at sign-in. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class StoreBillingTest {
    @get:Rule
    val compose = createComposeRule()

    private val server = MockCloud().start()

    @After
    fun stop() = server.stop()

    private class FakePlay(var owned: List<StorePurchase> = emptyList()) : StoreBilling {
        override val store = "google_play"
        val bought = mutableListOf<String>()
        override suspend fun buy(productId: String): StorePurchase { bought += productId; return StorePurchase(store, productId, purchaseToken = "ok-$productId") }
        override suspend fun active() = owned
    }

    private fun until(c: () -> Boolean) = compose.waitUntil(20_000) { ShadowLooper.idleMainLooper(); c() }

    @Test
    fun `CC21 a plan with a Play product is bought in the app and the server gives it`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context).apply { this.server = this@StoreBillingTest.server.url }
        val play = FakePlay()
        val vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5, billing = play)
        val nav = Navigator(Section.MORE)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val slice = noEngine(context)
        compose.setContent { TismaTheme { TismaApp(vm, printers, slice, settings, nav) } }
        compose.runOnIdle { vm.signIn("ana@example.com", "password1"); nav.open(Screen.Account) }
        until { vm.state.value.account?.plan == "Maker" && compose.onAllNodes(hasTestTag("plan_buy_Pro")).fetchSemanticsNodes().isNotEmpty() }
        // Only the plan sold in Play is offered.
        assertEquals(0, compose.onAllNodes(hasTestTag("plan_buy_Free")).fetchSemanticsNodes().size)
        compose.onNodeWithTag("plan_buy_Pro").performScrollTo().performClick()
        until { vm.state.value.account?.plan == "Pro" }
        assertEquals(listOf("pro_month"), play.bought)
        assertEquals("google_play", vm.state.value.account?.subscription?.store)
    }

    @Test
    fun `purchases the store already has are sent again at sign-in`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context).apply { this.server = this@StoreBillingTest.server.url }
        val play = FakePlay(listOf(StorePurchase("google_play", "pro_month", purchaseToken = "ok-restored")))
        val vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5, billing = play)
        vm.signIn("ana@example.com", "password1")
        val end = System.currentTimeMillis() + 20_000
        while (vm.state.value.account?.plan != "Pro" && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
        assertEquals("Pro", vm.state.value.account?.plan)
    }

    @Test
    fun `a purchase the store could not confirm yet (503) is kept and sent again`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context).apply { this.server = this@StoreBillingTest.server.url; pendingPurchases = emptySet() }
        val play = FakePlay()
        val vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5, billing = play)
        vm.signIn("ana@example.com", "password1")
        val end = System.currentTimeMillis() + 20_000
        while ((vm.state.value.account == null || vm.state.value.plans.isEmpty()) && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
        server.storeDown = 1
        vm.buy(vm.state.value.plans.first { it.name == "Pro" })
        while (settings.pendingPurchases.isEmpty() && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
        assertEquals(1, settings.pendingPurchases.size)
        assertEquals("Maker", vm.state.value.account?.plan)
        // The next refresh sends it again; the store answers now.
        vm.refreshAccount()
        while (vm.state.value.account?.plan != "Pro" && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
        assertEquals("Pro", vm.state.value.account?.plan)
        assertEquals(emptySet(), settings.pendingPurchases)
    }
}
