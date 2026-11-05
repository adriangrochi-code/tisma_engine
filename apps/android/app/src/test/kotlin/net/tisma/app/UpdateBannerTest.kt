package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import net.tisma.app.updates.AppUpdater
import net.tisma.app.updates.UpdatesViewModel
import net.tisma.mockcloud.StaticFiles
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** AU1, the phone's part: on opening, the newer version is found, downloaded, checked and offered for install. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class UpdateBannerTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private fun res(name: String) = javaClass.getResourceAsStream("/updates/$name")!!.readBytes()

    @Test
    fun `AU1 the notice says the version is ready and offers to install it`() {
        StaticFiles(listOf("updates.json", "updates.json.sig", "tisma-0.2.0.apk").associateWith { res(it) }, port = 47613).use { s ->
            val context: Context = ApplicationProvider.getApplicationContext()
            val settings = AppSettings(context)
            val updater = AppUpdater(net.tisma.cloud.UrlConnectionHttp, AppUpdater.hexKey(String(res("key.txt")))!!,
                listOf(s.url + "/updates.json"), tmp.newFolder("updates"))
            val updates = UpdatesViewModel(updater, settings, "0.1.0", 30)
            val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
            val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
            val slice = noEngine(context)
            val nav = Navigator()
            compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav, updates) } }
            wait("update_install")
            assertTrue(updates.state.value.ready!!.isFile)
            // Once a day: opening again the same day does not ask again.
            assertTrue(settings.updatesLastCheck > 0)
            val before = s.hits["updates.json"]
            updates.checkOnOpen()
            assertEquals(before, s.hits["updates.json"])
        }
    }

    private fun wait(tag: String) = compose.waitUntil(15_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    }
}
