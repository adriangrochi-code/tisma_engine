package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.cloud.CloudClient
import net.tisma.cloud.Http
import net.tisma.cloud.JobRequest
import net.tisma.cloud.JobStatus
import net.tisma.cloud.UrlConnectionHttp
import net.tisma.cloud.submit
import net.tisma.mockcloud.MockCloud
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import java.io.IOException
import java.util.concurrent.atomic.AtomicInteger
import kotlin.test.assertEquals

/** A job followed through a network loss keeps being followed when the network comes back. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class CloudOfflineTest {
    @Test
    fun `following a job survives lost answers`() {
        val server = MockCloud().start()
        try {
            val context: Context = ApplicationProvider.getApplicationContext()
            val settings = AppSettings(context).apply { this.server = server.url }
            val lost = AtomicInteger(0)
            // The phone loses the network for the next answers about jobs.
            val flaky = Http { r -> if (r.url.contains("/v1/jobs/") && lost.getAndDecrement() > 0) throw IOException("Network is unreachable") else UrlConnectionHttp.send(r) }
            val vm = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5,
                clientFor = { s, t -> CloudClient(s, t, flaky) })
            vm.signIn("ana@example.com", "password1")
            val end = System.currentTimeMillis() + 10_000
            while (!vm.state.value.signedIn && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
            val other = CloudClient(server.url).apply { login("ana@example.com", "password1", "t") }
            val bytes = cube3mf()
            val job = other.submit(JobRequest.Slice, "cube.3mf", bytes.size.toLong()) { bytes.inputStream() }
            lost.set(3)
            runBlocking { withTimeout(20_000) { vm.followJob(job.id) } }
            assertEquals(JobStatus.DONE, vm.state.value.jobs.first { it.id == job.id }.status)
        } finally {
            server.stop()
        }
    }
}
