package net.tisma.app

import android.content.Context
import androidx.compose.ui.semantics.SemanticsActions
import androidx.compose.ui.test.onRoot
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SettingsLayout
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Screen
import net.tisma.app.ui.Section
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import kotlin.test.assertTrue

/**
 * accessibility_touch.md, rule 9 (as AC7 on the desktop): on each main screen,
 * every control a finger can use has a name for the screen reader.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class AccessibilityTest {
    @get:Rule
    val compose = createComposeRule()

    @Test
    fun `rule 9 every control on the main screens has a name`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings,
            loadLayout = { SettingsLayout.parse(File("src/main/assets/settings/layout_en.json").readText()) })
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        val clickable = SemanticsMatcher("clickable") { it.config.getOrNull(SemanticsActions.OnClick) != null }
        val missing = mutableListOf<String>()
        val screens: List<Pair<String, () -> Unit>> = Section.entries.map { s -> s.name to { nav.select(s) } } + listOf(
            "Preferences" to { nav.select(Section.MORE); nav.open(Screen.Preferences) },
            "Print section" to { nav.select(Section.PLATE); nav.open(Screen.SettingsTab("print")); nav.open(Screen.SettingsSection("print", "quality")) },
            "Printer settings" to { nav.select(Section.PRINTER); nav.open(Screen.PrinterSettings) },
            "Report" to { nav.select(Section.MORE); nav.open(Screen.Report()) },
            "Account" to { nav.select(Section.MORE); nav.open(Screen.Account) },
            "Parts" to { nav.select(Section.ENGINEERING); nav.open(Screen.Parts) },
            "Calibrate" to { nav.select(Section.MORE); nav.open(Screen.Calibrate) },
            "Material" to { nav.select(Section.PRINTER); nav.open(Screen.SettingsTab("material")) },
            "CAD" to { nav.select(Section.ENGINEERING); nav.open(Screen.Cad) },
            "Help" to { nav.select(Section.MORE); nav.open(Screen.Help) },
        )
        for ((name, go) in screens) {
            compose.runOnIdle { go() }
            compose.waitForIdle()
            compose.onAllNodes(clickable).fetchSemanticsNodes().forEach { n ->
                val c = n.config
                val named = !c.getOrNull(SemanticsProperties.Text).isNullOrEmpty() ||
                    !c.getOrNull(SemanticsProperties.ContentDescription).isNullOrEmpty() ||
                    !c.getOrNull(SemanticsProperties.EditableText)?.text.isNullOrEmpty() ||
                    c.getOrNull(SemanticsProperties.ToggleableState) != null && n.children.isNotEmpty()
                if (!named) missing += "$name: ${c.getOrNull(SemanticsProperties.TestTag) ?: n.boundsInRoot}"
            }
        }
        assertTrue(missing.isEmpty(), "controls without a name:\n" + missing.joinToString("\n"))
    }

    /**
     * accessibility_touch.md, rule 5 (QA, beta): at twice the text size every main screen keeps its words whole —
     * no text is cut or runs out of its place.
     */
    @Test
    fun `rule 5 at twice the text size nothing is cut`() {
        org.robolectric.RuntimeEnvironment.setFontScale(2f)
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings,
            loadLayout = { SettingsLayout.parse(File("src/main/assets/settings/layout_en.json").readText()) })
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        val cut = mutableListOf<String>()
        val width = compose.onRoot(useUnmergedTree = true).fetchSemanticsNode().size.width.toFloat()
        val screens: List<Pair<String, () -> Unit>> = Section.entries.map { s -> s.name to { nav.select(s) } } + listOf(
            "Preferences" to { nav.select(Section.MORE); nav.open(Screen.Preferences) },
            "Report" to { nav.select(Section.MORE); nav.open(Screen.Report()) },
            "Help" to { nav.select(Section.MORE); nav.open(Screen.Help) },
        )
        for ((name, go) in screens) {
            compose.runOnIdle { go() }
            compose.waitForIdle()
            compose.onAllNodes(SemanticsMatcher("text") { it.config.getOrNull(SemanticsProperties.Text) != null })
                .fetchSemanticsNodes().forEach { n ->
                    // Robolectric does not measure glyphs, so the check is where the text sits: it has a size and stays
                    // inside the window's width (a cut or pushed-out label fails here).
                    var b = n.boundsInWindow
                    if (b.width <= 0f) runCatching {  // below the fold of a scrolling screen: bring it into view first
                        compose.onAllNodes(SemanticsMatcher("id") { it.id == n.id }, useUnmergedTree = false)[0].performScrollTo()
                        b = compose.onAllNodes(SemanticsMatcher("id") { it.id == n.id })[0].fetchSemanticsNode().boundsInWindow
                    }
                    val text = n.config.getOrNull(SemanticsProperties.Text)?.joinToString().orEmpty()
                    if (b.width <= 0f || b.height <= 0f || b.left < -1f || b.right > width + 1f) cut += "$name: «${text.take(40)}» $b"
                }
        }
        assertTrue(cut.isEmpty(), "text cut at twice the size:\n" + cut.joinToString("\n"))
    }

    @Test
    fun `rule 8 a notice stays with a close button and is listed in Messages`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val settings = AppSettings(context)
        val work = File(context.cacheDir, "engine")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val nav = Navigator()
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, nav) } }
        compose.runOnIdle { slice.open("bad.stl") { null } }
        val text = context.getString(R.string.error_cannot_read)
        compose.waitUntil(10_000) {
            org.robolectric.shadows.ShadowLooper.idleMainLooper()
            nav.notices.firstOrNull() == text
        }
        // Still there after the 4 s a short snackbar would last.
        compose.mainClock.advanceTimeBy(10_000)
        compose.onNodeWithText(text).assertExists()
        compose.runOnIdle { nav.select(Section.MORE); nav.open(Screen.Messages) }
        compose.onNodeWithTag("messages").assertExists()
        assertTrue(compose.onAllNodesWithText(text).fetchSemanticsNodes().size >= 1)
    }
}
