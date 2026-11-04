package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.onRoot
import androidx.compose.ui.test.printToString
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
import net.tisma.cloud.CloudClient
import net.tisma.cloud.FeaMode
import net.tisma.cloud.FeaQuality
import net.tisma.cloud.JobRequest
import net.tisma.cloud.submit
import net.tisma.mockcloud.MockCloud
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/** The cloud screens, drawn and used as a person would, against the test server. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class CloudScreensTest {
    @get:Rule
    val compose = createComposeRule()

    private val context: Context = ApplicationProvider.getApplicationContext()
    private lateinit var server: MockCloud
    private lateinit var settings: AppSettings
    private lateinit var vm: CloudViewModel
    private val nav = Navigator()

    @Before
    fun start() {
        server = MockCloud().start()
        settings = AppSettings(context).apply { server = this@CloudScreensTest.server.url }
        vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printersModel = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        val sliceModel = noEngine(context)
        compose.setContent { TismaTheme { TismaApp(vm, printersModel, sliceModel, settings, nav) } }
    }

    @After
    fun stop() = server.stop()

    private fun waitFor(tag: String) = try {
        compose.waitUntil(10_000) { compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty() }
    } catch (e: Throwable) {
        println("STATE " + vm.state.value)
        println(compose.onRoot().printToString())
        throw e
    }

    /** Waits for the model, letting the main thread run what it posted. */
    private fun waitState(condition: () -> Boolean) = compose.waitUntil(10_000) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        condition()
    }

    private fun waitForText(text: String) = compose.waitUntil(10_000) {
        compose.onAllNodes(hasText(text, substring = true), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()
    }

    @Test
    fun `CC1 and CC14 sign in from More and see the plan and the hours`() {
        server.grantHours("ana@example.com", 3.0)
        compose.onNodeWithTag("section_more").performClick()
        compose.onNodeWithTag("more_cloud").performClick()
        // CC13: the public plans are there before signing in; the hidden one is not.
        waitFor("plan_Pro")
        compose.onAllNodes(hasTestTag("plan_Staff")).fetchSemanticsNodes().let { assert(it.isEmpty()) }
        compose.onNodeWithTag("email").performTextInput("ana@example.com")
        compose.onNodeWithTag("password").performTextInput("password1")
        compose.onNodeWithTag("sign_in").performClick()
        waitFor("hours")
        // 5 h of the plan and 3 h given by the administration (rule 5a).
        compose.onNodeWithText("0 of 8 h used · 8 h left").assertIsDisplayed()
    }

    @Test
    fun `MA10 a cloud slice opens in the preview`() {
        compose.runOnIdle { vm.signIn("ana@example.com", "password1") }
        waitState { vm.state.value.signedIn }
        val client = CloudClient(server.url).apply { login("ana@example.com", "password1", "t") }
        val bytes = cube3mf()
        val job = client.submit(JobRequest.Slice, "cube.3mf", bytes.size.toLong()) { bytes.inputStream() }
        repeat(3) { client.job(job.id) }
        compose.runOnIdle { nav.select(Section.MORE); nav.open(Screen.JobDetail(job.id)) }
        waitFor("preview_result")
        // AND4: the summary in words, without the duplicate time.
        waitForText("Print time")
        waitForText("3.22 m")
        compose.onAllNodes(hasText("print_time_s", substring = true), useUnmergedTree = true).fetchSemanticsNodes()
            .let { assert(it.isEmpty()) }
        compose.onNodeWithTag("preview_result").performScrollTo().performClick()
        waitFor("preview_layer")
        waitForText("Layer 3 of 3")
        // Colored by speed: the mock's F1200 is 20 mm/s; it gives no ;WIDTH, so By width is off.
        compose.onNodeWithTag("preview_by_speed").performClick()
        waitFor("preview_ramp")
        waitForText("20 mm/s")
        compose.onNodeWithTag("preview_by_width").assertIsNotEnabled()
        compose.onNodeWithTag("preview_3d_toggle").performClick()
        waitFor("preview_3d")
    }

    @Test
    @Config(qualifiers = "es-w411dp-h891dp-xxhdpi")
    fun `CC20 a failed job shows error_es in Spanish`() {
        val client = CloudClient(server.url)
        client.login("pro@example.com", "password1", "test")
        val bytes = cube3mf()
        val job = client.submit(JobRequest.Fea(FeaMode.LIGHTEST_INFILL, FeaQuality.NORMAL, 0, 0), "cube.3mf",
            bytes.size.toLong()) { bytes.inputStream() }
        repeat(3) { client.job(job.id) }
        compose.runOnIdle { vm.signIn("pro@example.com", "password1") }
        waitState { vm.state.value.signedIn }
        compose.runOnIdle {
            nav.select(Section.MORE)
            nav.open(Screen.Jobs)
        }
        waitFor("job_error_" + job.id)
        compose.onNodeWithText("La búsqueda del relleno mínimo todavía no corre en la nube").assertIsDisplayed()
        compose.onNodeWithText("Falló").assertIsDisplayed()
    }

    @Test
    fun `CC16 a calculation outside the plan shows the lock with the plans that have it`() {
        compose.runOnIdle { vm.signIn("free@example.com", "password1") }
        waitState { vm.state.value.account != null && vm.state.value.plansLoaded }
        compose.runOnIdle {
            nav.select(Section.MORE)
            nav.open(Screen.NewJob)
        }
        waitFor("choice_aero")
        compose.onNodeWithTag("choice_aero").performClick()
        waitForText("Included in the plan Pro")
        compose.onNodeWithTag("choice_slice").performClick()
        compose.waitUntil(5_000) { compose.onAllNodes(hasTestTag("locked")).fetchSemanticsNodes().isEmpty() }
    }

    @Test
    fun `credits list the engine's and the app's libraries`() {
        compose.onNodeWithTag("section_more").performClick()
        compose.onNodeWithTag("more_credits").performScrollTo().performClick()
        waitFor("lib_oneTBB")
        waitFor("lib_kotlinx.coroutines")
    }

    @Test
    fun `a report without the title says what is missing and sends nothing`() {
        compose.onNodeWithTag("section_more").performClick()
        compose.onNodeWithTag("more_report").performClick()
        waitFor("report_send")
        compose.onNodeWithTag("report_send").performScrollTo().performClick()
        waitForText("Write in a few words what happened.")
        assert(server.reports.isEmpty())
        compose.onNodeWithTag("report_words").performScrollTo().performTextInput("Preview is empty")
        compose.onNodeWithTag("report_send").performScrollTo().performClick()
        waitState { server.reports.isNotEmpty() }
        val body = server.reports.single().body
        assert(body["app"].toString() == "\"android\"")
        // Attaching is on by default (problem_reports.md, rule 3).
        assert("log" in body && "os" in body && "app_version" in body)
    }
}
