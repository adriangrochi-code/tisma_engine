package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.mockcloud.MockCloud
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import kotlin.test.assertTrue

/** After signing out, nothing of that account is left for whoever signs in next on the phone. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SignOutTest {
    @Test
    fun `the sessions list goes with the account`() {
        val server = MockCloud().start()
        try {
            val context: Context = ApplicationProvider.getApplicationContext()
            val settings = AppSettings(context).apply { this.server = server.url }
            val vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
            val end = System.currentTimeMillis() + 10_000
            fun wait(c: () -> Boolean) { while (!c() && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) } }
            vm.signIn("ana@example.com", "password1")
            wait { vm.state.value.signedIn }
            vm.loadSessions()
            wait { vm.state.value.sessions.isNotEmpty() }
            vm.signOut()
            wait { !vm.state.value.signedIn }
            wait { vm.state.value.sessions.isEmpty() }
            assertTrue(vm.state.value.sessions.isEmpty())
        } finally {
            server.stop()
        }
    }
}
