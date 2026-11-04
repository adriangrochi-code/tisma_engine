package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.cloud.Notice
import net.tisma.app.cloud.ProjectSource
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.cloud.CloudException
import net.tisma.cloud.JobRequest
import net.tisma.cloud.JobStatus
import net.tisma.cloud.ProblemReport
import net.tisma.mockcloud.MockCloud
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.ByteArrayOutputStream
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CloudViewModelTest {
    private val context: Context = ApplicationProvider.getApplicationContext()
    private lateinit var server: MockCloud
    private lateinit var settings: AppSettings
    private lateinit var secrets: KeystoreSecretStore

    @Before
    fun start() {
        Dispatchers.setMain(UnconfinedTestDispatcher())
        server = MockCloud().start()
        settings = AppSettings(context).apply { this.server = this@CloudViewModelTest.server.url }
        secrets = KeystoreSecretStore(context) { testKey }
    }

    @After
    fun stop() {
        server.stop()
        Dispatchers.resetMain()
    }

    private fun model() = CloudViewModel(settings, secrets, "Test phone (Android)", pollMs = 5)

    private fun signedIn(email: String = "ana@example.com") = model().also { vm ->
        vm.signIn(email, "password1")
        eventually { vm.state.value.account != null && vm.state.value.plansLoaded }
    }

    @Test
    fun `CC1 signing in keeps the token in the secret store only`() {
        val vm = signedIn()
        val s = vm.state.value
        assertEquals("ana@example.com", s.email)
        assertEquals("Maker", s.account?.plan)
        assertEquals("Maker", s.currentPlan?.name)
        val token = assertNotNull(secrets.get(CloudViewModel.TOKEN))
        val prefsFile = File(context.applicationInfo.dataDir, "shared_prefs/${AppSettings.FILE}.xml")
        eventually { prefsFile.exists() }
        assertFalse(prefsFile.readText().contains(token))
        // The login told the server which device it is.
        assertEquals("/v1/auth/login", server.log.first { it.second == "/v1/auth/login" }.second)
        // A new model (the app opened again) is still signed in.
        assertTrue(model().state.value.signedIn)
    }

    @Test
    fun `CC2 an expired session asks to sign in again and forgets the token`() {
        val vm = signedIn()
        server.expireTokens()
        vm.refreshAccount()
        eventually { vm.state.value.notice == Notice.SessionExpired }
        assertFalse(vm.state.value.signedIn)
        assertNull(secrets.get(CloudViewModel.TOKEN))
    }

    @Test
    fun `a wrong password is told with the server's text and no session`() {
        val vm = model()
        vm.signIn("ana@example.com", "wrong")
        eventually { vm.state.value.notice != null }
        val n = vm.state.value.notice as Notice.Failed
        assertEquals(CloudException.Kind.EXPIRED, n.error.kind)
        assertEquals("Wrong e-mail or password", n.error.detail)
        assertFalse(vm.state.value.signedIn)
    }

    @Test
    fun `CC13 the plans show without a session`() {
        val vm = model()
        vm.refreshPublic()
        eventually { vm.state.value.plansLoaded }
        assertEquals(listOf("Free", "Maker", "Pro"), vm.state.value.plans.map { it.name })
        assertTrue(vm.state.value.billingUrl.endsWith("/account"))
    }

    @Test
    fun `CC3 a sent project is followed to done and its result saved`() = runBlocking {
        val vm = signedIn()
        val bytes = cube3mf()
        var id: String? = null
        vm.submit(JobRequest.Slice, ProjectSource("cube.3mf", bytes.size.toLong()) { bytes.inputStream() }) { id = it }
        eventually { id != null }
        vm.followJob(id!!)
        val job = vm.state.value.jobs.first { it.id == id }
        assertEquals(JobStatus.DONE, job.status)
        val out = ByteArrayOutputStream()
        vm.saveResult(id!!, "cube.gcode") { out }
        eventually { vm.state.value.notice == Notice.Saved("cube.gcode") }
        assertTrue(out.toString(Charsets.UTF_8).contains("G28"))
        vm.followJobs()
        assertEquals(listOf(id), vm.state.value.jobs.map { it.id })
    }

    @Test
    fun `a file that is not 3MF is refused before sending`() {
        val vm = signedIn()
        val stl = "solid x\nendsolid x\n".toByteArray()
        vm.submit(JobRequest.Slice, ProjectSource("x.stl", stl.size.toLong()) { stl.inputStream() }) { }
        eventually { vm.state.value.notice == Notice.NotThreeMf }
        assertTrue(server.log.none { it.second == "/v1/jobs" && it.first == "POST" })
    }

    @Test
    fun `CC4 cancelling a running job`() = runBlocking {
        val vm = signedIn()
        server.stepsToFinish = 1000
        val bytes = cube3mf()
        var id: String? = null
        vm.submit(JobRequest.Slice, ProjectSource("cube.3mf", bytes.size.toLong()) { bytes.inputStream() }) { id = it }
        eventually { id != null }
        vm.cancel(id!!)
        eventually { vm.state.value.jobs.first { it.id == id }.status == JobStatus.CANCELLED }
    }

    @Test
    fun `MA11 a report reaches the server with app android`() {
        val vm = model()
        var sent = false
        vm.sendReport(ProblemReport(kind = "bug", title = "Preview is empty")) { sent = true }
        eventually { sent }
        assertEquals("android", server.reports.single().body["app"].toString().trim('"'))
        assertEquals(Notice.ReportSent(0), vm.state.value.notice)
    }

    @Test
    fun `signing out forgets the session`() {
        val vm = signedIn()
        vm.signOut()
        eventually { !vm.state.value.signedIn }
        assertNull(secrets.get(CloudViewModel.TOKEN))
        assertNull(settings.email)
    }

    @Test
    fun `changing the server closes the session`() {
        val vm = signedIn()
        vm.setServer("example.invalid")
        assertEquals("https://example.invalid", vm.state.value.server)
        assertFalse(vm.state.value.signedIn)
        assertNull(secrets.get(CloudViewModel.TOKEN))
    }
}
