package net.tisma.app

import net.tisma.app.updates.AppUpdater
import net.tisma.app.updates.Updates
import net.tisma.mockcloud.StaticFiles
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The updater against a local server (app_updates.md; AU2, AU4, AU12). The
 * list, its signature and the package were signed with a test key by another
 * Ed25519 implementation; the list names the package on port 47613.
 */
class AppUpdaterTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun res(name: String) = javaClass.getResourceAsStream("/updates/$name")!!.readBytes()
    private val key = AppUpdater.hexKey(String(res("key.txt")))!!
    private val apk = res("tisma-0.2.0.apk")

    private fun files(apkBytes: ByteArray = apk) = mapOf("updates.json" to res("updates.json"),
        "updates.json.sig" to res("updates.json.sig"), "tisma-0.2.0.apk" to apkBytes)

    @Test
    fun `AU12 the list comes from the second address when the first does not answer, then the package downloads and checks`() {
        StaticFiles(files(), port = 47613).use { s ->
            val u = AppUpdater(net.tisma.cloud.UrlConnectionHttp, key, listOf(s.url + "/missing/updates.json", s.url + "/updates.json"),
                tmp.newFolder("updates"))
            val choice = Updates.choose(u.releases(), "stable", 30, "0.1.0")
            val offer = choice.offer!!
            assertEquals("0.2.0", offer.release.version)
            val progress = mutableListOf<Int>()
            val f = u.download(offer, { progress += it }) { false }
            assertTrue(f.readBytes().contentEquals(apk))
            assertEquals(100, progress.last())
        }
    }

    @Test
    fun `AU4 a cut download goes on from where it stopped`() {
        StaticFiles(files(), port = 47613).use { s ->
            val dir = tmp.newFolder("updates")
            java.io.File(dir, "tisma-0.2.0.apk.part").writeBytes(apk.copyOf(25_000))
            val u = AppUpdater(net.tisma.cloud.UrlConnectionHttp, key, listOf(s.url + "/updates.json"), dir)
            val offer = Updates.choose(u.releases(), "stable", 30, "0.1.0").offer!!
            val f = u.download(offer, {}) { false }
            assertTrue(f.readBytes().contentEquals(apk))
            assertEquals(listOf("bytes=25000-"), s.ranges.toList())
        }
    }

    @Test
    fun `AU2 a package with one byte changed is deleted and not offered for install`() {
        val bad = apk.copyOf().also { it[30_000] = (it[30_000].toInt() xor 1).toByte() }
        StaticFiles(files(bad), port = 47613).use { s ->
            val dir = tmp.newFolder("updates")
            val u = AppUpdater(net.tisma.cloud.UrlConnectionHttp, key, listOf(s.url + "/updates.json"), dir)
            val offer = Updates.choose(u.releases(), "stable", 30, "0.1.0").offer!!
            val e = assertFailsWith<AppUpdater.UpdateException> { u.download(offer, {}) { false } }
            assertEquals("damaged", e.message)
            assertFalse(dir.listFiles()!!.any { it.name.endsWith(".apk") || it.name.endsWith(".part") })
        }
    }

    @Test
    fun `a package longer than the list says stops at its size, and the fingerprint is read in any case`() {
        StaticFiles(files(apk + ByteArray(2 shl 20)), port = 47613).use { s ->
            val dir = tmp.newFolder("updates")
            val u = AppUpdater(net.tisma.cloud.UrlConnectionHttp, key, listOf(s.url + "/updates.json"), dir)
            val offer = Updates.choose(u.releases(), "stable", 30, "0.1.0").offer!!
            val e = assertFailsWith<AppUpdater.UpdateException> { u.download(offer, {}) { false } }
            assertEquals("damaged", e.message)
            assertFalse(dir.listFiles()!!.any { it.name.endsWith(".part") && it.length() > offer.pkg.size })
            val good = tmp.newFile("good.apk").apply { writeBytes(apk) }
            assertTrue(u.check(good, offer.pkg.copy(sha256 = offer.pkg.sha256.uppercase())))
        }
    }
}
