package net.tisma.app

import android.content.Context
import android.graphics.Bitmap
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.test.captureToImage
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onRoot
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Screen
import net.tisma.app.ui.Section
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import net.tisma.mockcloud.MockCloud
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue

/**
 * Draws the account screen in both themes to build/screenshots/, for looking
 * at the design; it checks only that drawing works.
 */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35], qualifiers = "es-w411dp-h891dp-xxhdpi")
class ScreenshotTest {
    @get:Rule
    val compose = createAndroidComposeRule<androidx.activity.ComponentActivity>()

    @Test
    fun `account screen in light and dark`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val mock = MockCloud().start()
        try {
            val settings = AppSettings(context).apply { server = mock.url }
            val vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test", pollMs = 5)
            val nav = Navigator()
            var dark by androidx.compose.runtime.mutableStateOf(false)
            val printersModel = PrintersViewModel(PrinterStore(context, MemorySecrets()))
            val sliceModel = noEngine(context)
            compose.setContent { TismaTheme(dark = dark) { TismaApp(vm, printersModel, sliceModel, settings, nav) } }
            compose.runOnIdle { vm.signIn("ana@example.com", "password1") }
            compose.waitUntil(10_000) {
                org.robolectric.shadows.ShadowLooper.idleMainLooper()
                vm.state.value.account != null && vm.state.value.plansLoaded
            }
            compose.runOnIdle { nav.select(Section.MORE); nav.open(Screen.Account) }
            compose.waitUntil(10_000) { compose.onAllNodes(hasTestTag("hours"), true).fetchSemanticsNodes().isNotEmpty() }
            val dir = File("build/screenshots").apply { mkdirs() }
            for (d in listOf(false, true)) {
                compose.runOnIdle { dark = d }
                compose.waitForIdle()
                val view = compose.activity.window.decorView
                val bmp = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
                compose.runOnIdle { view.draw(android.graphics.Canvas(bmp)) }
                File(dir, "account_${if (d) "dark" else "light"}.png").outputStream().use {
                    bmp.compress(Bitmap.CompressFormat.PNG, 100, it)
                }
            }
        } finally {
            mock.stop()
        }
    }

    /** AC3 on the phone (accessibility_touch.md, rule 5): the main screens with the text at 200 %, to look at. */
    @Test
    fun `main screens with the text at 200 percent`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = net.tisma.app.engine.SliceViewModel(net.tisma.app.engine.EngineRunner(File(work, "none"), work), work,
            saved = settings, loadLayout = {
                net.tisma.app.engine.SettingsLayout.parse(File("src/main/assets/settings/layout_es.json").readText())
            })
        val vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test", pollMs = 5)
        val printersModel = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent {
            val d = androidx.compose.ui.platform.LocalDensity.current
            androidx.compose.runtime.CompositionLocalProvider(
                androidx.compose.ui.platform.LocalDensity provides androidx.compose.ui.unit.Density(d.density, 2f)) {
                TismaTheme { TismaApp(vm, printersModel, slice, settings, nav) }
            }
        }
        val dir = File("build/screenshots").apply { mkdirs() }
        listOf<Pair<String, () -> Unit>>(
            "plate" to { nav.select(Section.PLATE) },
            "print" to { nav.select(Section.PLATE); nav.open(Screen.SettingsTab("print")) },
            "print_quality" to { nav.select(Section.PLATE); nav.open(Screen.SettingsTab("print")); nav.open(Screen.SettingsSection("print", "quality")) },
            "more" to { nav.select(Section.MORE) },
            "printer" to { nav.select(Section.PRINTER) },
        ).forEach { (name, go) ->
            compose.runOnIdle { go() }
            compose.waitForIdle()
            val view = compose.activity.window.decorView
            val bmp = Bitmap.createBitmap(view.width, view.height, Bitmap.Config.ARGB_8888)
            compose.runOnIdle { view.draw(android.graphics.Canvas(bmp)) }
            File(dir, "text200_$name.png").outputStream().use { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }
        }
    }
}
