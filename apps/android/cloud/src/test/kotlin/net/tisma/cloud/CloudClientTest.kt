package net.tisma.cloud

import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.runBlocking
import net.tisma.mockcloud.MockCloud
import org.junit.After
import org.junit.Before
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The client against the test server written from cloud.md (CC1 to CC6, CC13 to CC16, CC20). */
class CloudClientTest {
    private lateinit var server: MockCloud
    private lateinit var client: CloudClient

    @Before
    fun start() {
        server = MockCloud().start()
        client = CloudClient(server.url)
    }

    @After
    fun stop() = server.stop()

    private fun signIn(email: String = "ana@example.com") = client.login(email, "password1", "Test phone")

    private fun submit(request: JobRequest, bytes: ByteArray = cube3mf()) =
        client.submit(request, "cube.3mf", bytes.size.toLong()) { bytes.inputStream() }

    @Test
    fun `CC1 signing in gives a token that every request then carries`() {
        val s = signIn()
        assertTrue(s.token.isNotEmpty())
        assertEquals("ana@example.com", s.email)
        assertEquals(s.token, client.token)
        client.account()
        val (_, path, auth) = server.log.last()
        assertEquals("/v1/me", path)
        assertEquals("Bearer ${s.token}", auth)
    }

    @Test
    fun `a wrong password is a 401 with the server's text`() {
        val e = assertFailsWith<CloudException> { client.login("ana@example.com", "nope", "Test phone") }
        assertEquals(401, e.status)
        assertEquals("Wrong e-mail or password", e.detail)
        assertNull(client.token)
    }

    @Test
    fun `CC2 an expired token is a 401 that asks to sign in again`() {
        signIn()
        server.expireTokens()
        val e = assertFailsWith<CloudException> { client.account() }
        assertEquals(CloudException.Kind.EXPIRED, e.kind)
    }

    @Test
    fun `CC15 a disabled account fails its next request with 401`() {
        signIn()
        client.account()
        server.disable("ana@example.com")
        assertEquals(CloudException.Kind.EXPIRED, assertFailsWith<CloudException> { client.jobs() }.kind)
    }

    @Test
    fun `CC13 the public plans come without a session and a hidden plan does not`() {
        val plans = client.plans()
        assertEquals(listOf("Free", "Maker", "Pro"), plans.map { it.name })
        val maker = plans[1]
        assertEquals(5.0, maker.monthlyHours)
        assertEquals(2, maker.maxConcurrent)
        assertEquals(900, maker.priceCents)
        assertEquals("usd", maker.currency)
        assertTrue(maker.includes("fea"))
        assertFalse(maker.includes("aero"))
        assertNull(server.log.last().third)
    }

    @Test
    fun `CC14 hours given by the administration show as they come`() {
        signIn()
        assertEquals(5.0, client.account().hoursIncluded)
        server.grantHours("ana@example.com", 3.0)
        val a = client.account()
        assertEquals(8.0, a.hoursIncluded)
        assertEquals("Maker", a.plan)
        assertEquals(2, a.maxConcurrent)
    }

    @Test
    fun `CC3 a slice goes from queued through running to done and its G-code downloads`() = runBlocking {
        signIn()
        val job = submit(JobRequest.Slice)
        assertEquals(JobStatus.QUEUED, job.status)
        assertEquals("slice", job.kind)
        val seen = client.watch(job.id, intervalMs = 1).toList()
        assertEquals(listOf(JobStatus.RUNNING, JobStatus.RUNNING, JobStatus.DONE), seen.map { it.status })
        assertTrue(seen.map { it.progress }.zipWithNext().all { (a, b) -> a <= b })
        val done = seen.last()
        assertEquals(100, done.progress)
        assertEquals("1122", done.summary["print_time_s"]?.display())
        assertEquals("project.gcode", done.resultFilename)
        val out = ByteArrayOutputStream()
        val d = client.download(job.id, out)
        assertEquals("project.gcode", d.filename)
        assertEquals(out.size().toLong(), d.bytes)
        assertTrue(out.toString(Charsets.UTF_8).contains("G28"))
        // The list has it, newest first.
        submit(JobRequest.Slice)
        val list = client.jobs()
        assertEquals(2, list.size)
        assertEquals(job.id, list[1].id)
    }

    @Test
    fun `AND1 uploads carry Content-Length, also when the file's size is unknown`() {
        signIn()
        val bytes = cube3mf()
        val job = client.submit(JobRequest.Slice, "cube.3mf", -1) { bytes.inputStream() }
        assertEquals(JobStatus.QUEUED, job.status)
        val parts = listOf(FormPart.text("kind", "slice"), FormPart("file", "x", "a.3mf", -1) { bytes.inputStream() })
        val out = java.io.ByteArrayOutputStream()
        writeMultipart(out, "b", parts)
        assertEquals(out.size().toLong(), multipartLength("b", parts))
    }

    @Test
    fun `CC4 cancelling a running job leaves it cancelled`() {
        signIn()
        val job = submit(JobRequest.Slice)
        assertEquals(JobStatus.RUNNING, client.job(job.id).status)
        client.cancel(job.id)
        assertEquals(JobStatus.CANCELLED, client.job(job.id).status)
    }

    @Test
    fun `CC5 a project over the limit is a 413`() {
        signIn()
        server.maxUploadBytes = 1000
        val big = cube3mf(padding = 50_000)
        val e = assertFailsWith<CloudException> { submit(JobRequest.Slice, big) }
        assertEquals(CloudException.Kind.TOO_LARGE, e.kind)
        assertEquals("The file is too large", e.detail)
    }

    @Test
    fun `a file that is not a 3MF package is a 422, and the client can tell first`() {
        signIn()
        val stl = "solid cube\nendsolid cube\n".toByteArray()
        assertFalse(isThreeMfPackage(stl.inputStream()))
        assertTrue(isThreeMfPackage(cube3mf().inputStream()))
        assertTrue(looksLikeZip(cube3mf().inputStream()))
        assertFalse(looksLikeZip(stl.inputStream()))
        assertFalse(looksLikeZip(ByteArray(2).inputStream()))
        val e = assertFailsWith<CloudException> { submit(JobRequest.Slice, stl) }
        assertEquals(CloudException.Kind.BAD_PARAMS, e.kind)
        assertEquals("The project is not a 3MF file", e.detail)
    }

    @Test
    fun `CC16 a calculation the plan does not include is locked, and the server says 402`() {
        signIn("free@example.com")
        val plans = client.plans()
        val free = plans.first { it.name == client.account().plan }
        val aero = JobRequest.Aero(0, 0, AeroQuality.FAST, 10.0, AeroDirection.X)
        assertFalse(free.allows(aero))
        assertTrue(free.allows(JobRequest.Slice))
        assertEquals(listOf("Pro"), plans.including(aero).map { it.name })
        val e = assertFailsWith<CloudException> { submit(aero) }
        assertEquals(CloudException.Kind.PLAN, e.kind)
        assertEquals("Included in the plan Pro", e.detail)
    }

    @Test
    fun `CC20 a job that cannot run in the cloud fails with code and error_es`() = runBlocking {
        signIn()
        server.setPlan("ana@example.com", "Pro")
        val job = submit(JobRequest.Fea(FeaMode.LIGHTEST_INFILL, FeaQuality.NORMAL, 0, 0))
        assertEquals("fea_optimize", job.jobClass)
        val last = client.watch(job.id, intervalMs = 1).toList().last()
        assertEquals(JobStatus.FAILED, last.status)
        val f = assertNotNull(last.failure())
        assertEquals("not_in_cloud", f.code)
        assertTrue(f.error.isNotEmpty())
        val codeText = { c: String -> if (c == "not_in_cloud") "texto por código" else null }
        assertEquals(f.errorEs, f.message(spanish = true, codeText))
        assertEquals("texto por código", f.message(spanish = false, codeText))
        assertEquals(f.error, f.copy(code = "error").message(spanish = false, codeText))
        assertEquals("texto por código", f.copy(errorEs = "").message(spanish = true, codeText))
        assertEquals(listOf<Pair<String, String>>(), last.summaryLines())
    }

    @Test
    fun `fea and aero send the params of rule 6`() {
        signIn("pro@example.com")
        val fea = submit(JobRequest.Fea(FeaMode.ANALYZE, FeaQuality.HIGH, 1, 2))
        assertEquals("""{"mode":"analyze","quality":"high","object":1,"instance":2}""", fea.params.toString())
        val aero = submit(JobRequest.Aero(0, 0, AeroQuality.NORMAL, 12.5, AeroDirection.MINUS_Y))
        assertEquals("""{"object":0,"instance":0,"quality":"normal","speed":12.5,"direction":"-y"}""", aero.params.toString())
        assertFailsWith<IllegalArgumentException> { JobRequest.Aero(0, 0, AeroQuality.FAST, 0.0, AeroDirection.X) }
        assertFailsWith<IllegalArgumentException> { JobRequest.Aero(0, 0, AeroQuality.FAST, 101.0, AeroDirection.X) }
    }

    @Test
    fun `429 is told apart`() {
        server.rateLimitPerMinute = 2
        client.plans()
        client.plans()
        assertEquals(CloudException.Kind.TOO_MANY, assertFailsWith<CloudException> { client.plans() }.kind)
    }

    @Test
    fun `no server is a network error`() {
        server.stop()
        assertEquals(CloudException.Kind.NETWORK, assertFailsWith<CloudException> { client.plans() }.kind)
        server = MockCloud().start() // for @After
    }

    @Test
    fun `CC6 a report goes without an account, and without the log when not accepted`() {
        val similar = client.report(ProblemReport(kind = "bug", title = "Supports missing", description = "Under the arch"))
        assertEquals(0, similar)
        val sent = server.reports.single()
        assertNull(sent.owner)
        assertEquals("android", sent.body["app"]?.display())
        assertFalse("log" in sent.body)
        assertFalse("app_version" in sent.body)
        assertFalse("os" in sent.body)
        // The same problem again counts the earlier one.
        assertEquals(1, client.report(ProblemReport(kind = "bug", title = "supports missing")))
        val short = assertFailsWith<CloudException> { client.report(ProblemReport(kind = "bug", title = "ab")) }
        assertEquals(CloudException.Kind.BAD_PARAMS, short.kind)
    }

    @Test
    fun `a signed-in report is listed in my reports`() {
        signIn()
        client.report(ProblemReport(kind = "suggestion", title = "Dark preview", appVersion = "0.1.0", os = "Android 15",
            log = "line\n".repeat(50_000)))
        val body = server.reports.single().body
        assertEquals(190_000, body["log"]!!.display().length)
        assertEquals("Android 15", body["os"]?.display())
        val mine = client.myReports()
        assertEquals("Dark preview", mine.single().title)
        assertEquals("new", mine.single().status)
    }

    @Test
    fun `register, verify, reset and log out`() {
        client.register("new@example.com", "longpassword")
        assertFailsWith<CloudException> { client.login("new@example.com", "longpassword", "Test phone") }
        val s = client.verify("new@example.com", server.mailedCode, "Test phone")
        assertEquals("new@example.com", s.email)
        assertEquals("Free", client.account().plan)
        client.logout()
        assertNull(client.token)
        client.resetPassword("new@example.com", server.mailedCode, "otherpassword")
        client.login("new@example.com", "otherpassword", "Test phone")
        val e = assertFailsWith<CloudException> { client.register("x@example.com", "short") }
        assertEquals("The password needs 8 or more characters", e.detail)
    }

    @Test
    fun `sessions are listed with the current one, and the others can be closed`() {
        signIn()
        val other = CloudClient(server.url).apply { login("ana@example.com", "password1", "Tablet") }
        val list = client.sessions()
        assertEquals(2, list.size)
        assertEquals("Test phone", list.single { it.current }.device)
        assertEquals(1, client.closeOtherSessions())
        assertEquals(CloudException.Kind.EXPIRED, assertFailsWith<CloudException> { other.account() }.kind)
        val third = CloudClient(server.url).apply { login("ana@example.com", "password1", "Laptop") }
        client.closeSession(client.sessions().single { !it.current }.id)
        assertEquals(CloudException.Kind.EXPIRED, assertFailsWith<CloudException> { third.account() }.kind)
        assertTrue(client.account().hasPassword)
    }

    @Test
    fun `the server address gets https and loses trailing slashes`() {
        assertEquals("https://tismaservidores.net", CloudClient.normalizeServer("tismaservidores.net/"))
        assertEquals("http://10.0.2.2:8765", CloudClient.normalizeServer(" http://10.0.2.2:8765// "))
        assertEquals(CloudClient.DEFAULT_SERVER, CloudClient.normalizeServer(""))
    }

    @Test
    fun `server error texts are read from detail or the first validation message`() {
        assertEquals("Bad", CloudException.detailOf("""{"detail":"Bad"}""", 400))
        assertEquals("field required", CloudException.detailOf("""{"detail":[{"msg":"field required","loc":["x"]}]}""", 422))
        assertEquals("HTTP 502", CloudException.detailOf("<html>bad gateway</html>", 502))
    }

    companion object {
        /** A minimal 3MF package: a ZIP with `3D/3dmodel.model`. */
        fun cube3mf(padding: Int = 0): ByteArray {
            val out = ByteArrayOutputStream()
            ZipOutputStream(out).use { z ->
                z.putNextEntry(ZipEntry("[Content_Types].xml"))
                z.write("<Types/>".toByteArray())
                z.putNextEntry(ZipEntry("3D/3dmodel.model"))
                z.write("<model unit=\"millimeter\"><resources/><build/></model>".toByteArray())
                if (padding > 0) {
                    // Stored, so the bytes stay that many.
                    val e = ZipEntry("Metadata/padding.bin")
                    val data = ByteArray(padding) { (it * 31 + 7).toByte() }
                    e.method = ZipEntry.STORED
                    e.size = data.size.toLong()
                    e.crc = java.util.zip.CRC32().apply { update(data) }.value
                    z.putNextEntry(e)
                    z.write(data)
                }
            }
            return out.toByteArray()
        }
    }
}
