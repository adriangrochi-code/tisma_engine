package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.p2p.BlockList
import net.tisma.app.p2p.LibraryIndex
import net.tisma.app.p2p.LibraryKey
import net.tisma.app.p2p.LibraryServer
import net.tisma.app.p2p.SharedCopies
import net.tisma.app.p2p.SharedViewModel
import net.tisma.app.p2p.SharingPolicy
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.Navigator
import net.tisma.app.ui.Section
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import java.io.File
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.Signature
import kotlin.time.Clock
import kotlin.time.Duration.Companion.days
import kotlin.time.Instant
import java.util.Base64
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.test.assertFalse

/**
 * p2p_library.md: the key chain (project key → library key), the signed block list and index, applied to the
 * phone's shared copies, «Denunciar», and when the phone may share.
 */
@OptIn(kotlin.time.ExperimentalTime::class)
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class BlockListTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private fun pair(): KeyPair = KeyPairGenerator.getInstance("Ed25519").generateKeyPair()
    /** The raw 32-byte public key (the end of its X.509 encoding). */
    private fun raw(k: KeyPair) = k.public.encoded.takeLast(32).toByteArray()
    private fun sign(k: KeyPair, b: ByteArray): ByteArray = Signature.getInstance("Ed25519").run { initSign(k.private); update(b); sign() }

    private val project = pair()
    private val library = pair()
    private val projectKey = raw(project)
    private val libraryKey = raw(library)

    private fun keyJson(lib: KeyPair = library, certifier: KeyPair = project): ByteArray {
        val b64 = Base64.getEncoder()
        val cert = sign(certifier, "tisma-library-key/1\n".toByteArray() + raw(lib))
        return """{"public_key":"${b64.encodeToString(raw(lib))}","certificate":"${b64.encodeToString(cert)}"}""".toByteArray()
    }

    private fun list(sequence: Long, generated: Instant = Clock.System.now(), files: List<String> = emptyList(),
                     revoked: List<String> = emptyList()) = """{"format":"tisma-library-blocklist","version":1,
        "generated":"$generated","sequence":$sequence,"expires":"${generated + 7.days}",
        "files":[${files.joinToString(",") { "{\"sha256\":\"$it\",\"reason\":\"rights\"}" }}],
        "accounts":["acct-9"],"peers":["peer-1"],"addresses":[],"models":[{"id":"m-blocked","reason":"rights"}],
        "revoked_keys":[${revoked.joinToString(",") { "\"$it\"" }}]}""".toByteArray()

    private fun model(id: String, name: String, account: String, sha: String, extra: String = "") =
        """{"id":"$id","revision":1,"name":"$name","description":"","tags":["gear"],"license":"CC-BY-4.0",
        "author":{"name":"Ana","account":"$account"},"size":120,"checked":true,"thumbnail":"",
        "files":[{"path":"model.3mf","role":"model","size":120,"sha256":"$sha"}$extra]}"""

    private fun index(sequence: Long, vararg models: String) =
        """{"format":"tisma-library-index","version":1,"generated":"${Clock.System.now()}","sequence":$sequence,
        "models":[${models.joinToString(",")}]}""".toByteArray()

    @Test
    fun `rule 3 quinquies the library key counts only with the project's certificate`() {
        assertTrue(LibraryKey.read(keyJson(), projectKey).contentEquals(libraryKey))
        assertFailsWith<LibraryKey.BadKey> { LibraryKey.read(keyJson(certifier = pair()), projectKey) }
        // No project key in the build yet: nothing is accepted.
        assertFailsWith<LibraryKey.BadKey> { LibraryKey.read(keyJson(), ByteArray(0)) }
        assertFailsWith<LibraryKey.BadKey> { LibraryKey.read("""{"public_key":"00"}""".toByteArray(), projectKey) }
    }

    @Test
    fun `P2P5 a list is used only with a valid signature, a newer sequence and an unrevoked key`() {
        val json = list(5)
        val l = BlockList.read(json, sign(library, json), libraryKey)
        assertTrue(l.blocksAccount("acct-9"))
        assertTrue(l.blocksPeer("peer-1"))
        assertTrue(l.blocksModel("m-blocked"))
        assertFailsWith<BlockList.BadList> { BlockList.read(list(5, files = listOf("ab")), sign(library, json), libraryKey) }
        assertFailsWith<BlockList.BadList> { BlockList.read(json, sign(pair(), json), libraryKey) }
        // An older list is a replay.
        assertFailsWith<BlockList.BadList> { BlockList.read(json, sign(library, json), libraryKey, lastSequence = 6) }
        val revoked = list(7, revoked = listOf(LibraryKey.hex(libraryKey)))
        assertFailsWith<BlockList.BadList> { BlockList.read(revoked, sign(library, revoked), libraryKey) }
        assertEquals("rights", l.models["m-blocked"])
    }

    @Test
    fun `P2P7 an expired list stops sharing, and P2P6 only on Wi-Fi and charging`() {
        val now = Clock.System.now()
        val fresh = list(1, now).let { BlockList.read(it, sign(library, it), libraryKey) }
        val old = list(1, now - 8.days).let { BlockList.read(it, sign(library, it), libraryKey) }
        assertEquals(SharingPolicy.Why.STALE_LIST, SharingPolicy.blocked(true, old, unmeteredNetwork = true, charging = true, now))
        assertEquals(SharingPolicy.Why.NOT_WIFI, SharingPolicy.blocked(true, fresh, unmeteredNetwork = false, charging = true, now))
        assertEquals(SharingPolicy.Why.NOT_CHARGING, SharingPolicy.blocked(true, fresh, unmeteredNetwork = true, charging = false, now))
        assertNull(SharingPolicy.blocked(true, fresh, unmeteredNetwork = true, charging = true, now))
        assertEquals(SharingPolicy.Why.NO_LIST, SharingPolicy.blocked(true, null, true, true, now))
        assertEquals(SharingPolicy.Why.OFF, SharingPolicy.blocked(false, fresh, true, true, now))
    }

    @Test
    fun `rule 3 ter the index is signed, searched, and leaves out blocked accounts, files and G-code`() {
        val bad = "b".repeat(64)
        val json = index(3,
            model("m1", "Gear box", "acct-1", "a".repeat(64)),
            model("m2", "Gear blocked author", "acct-9", "c".repeat(64)),
            model("m3", "Gear blocked file", "acct-1", bad.uppercase()),
            model("m4", "Gear with gcode", "acct-1", "d".repeat(64), """,{"path":"plate.gcode","role":"gcode","size":1,"sha256":"${"e".repeat(64)}"}"""))
        val idx = LibraryIndex.read(json, sign(library, json), libraryKey)
        assertEquals(4, idx.models.size)
        val l = list(1, files = listOf(bad)).let { BlockList.read(it, sign(library, it), libraryKey) }
        assertEquals(listOf("m1"), idx.search("gear", l).map { it.id })
        assertEquals(listOf("m1"), idx.search("ana cc-by", l).map { it.id })
        assertTrue(idx.search("nothing-like-this", l).isEmpty())
        assertFailsWith<LibraryIndex.BadIndex> { LibraryIndex.read(json, sign(pair(), json), libraryKey) }
        assertFailsWith<LibraryIndex.BadIndex> { LibraryIndex.read(json, sign(library, json), libraryKey, lastSequence = 4) }
    }

    private class FakeServer(val files: Map<String, ByteArray>) : LibraryServer {
        val reports = mutableListOf<Triple<String?, String?, String>>()
        override fun file(name: String) = files[name] ?: throw java.io.IOException("404 $name")
        override fun report(model: String?, sha256: String?, reason: String): String { reports += Triple(model, sha256, reason); return "r1" }
    }

    @Test
    fun `P2P3 a withdrawn file is deleted from the phone and said, and rule 7 Denunciar reaches the server`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val dir = tmp.newFolder("p2p")
        val copies = SharedCopies(File(dir, "files"))
        val bad = copies.add(tmp.newFile("bad.stl").apply { writeText("solid bad\nendsolid bad\n") }, "stl")
        val good = copies.add(tmp.newFile("good.stl").apply { writeText("solid good\nendsolid good\n") }, "stl")
        val json = list(2, files = listOf(bad.sha256.uppercase()))
        val idx = index(1, model("m1", "Gear box", "acct-1", good.sha256))
        val server = FakeServer(mapOf("key" to keyJson(), "blocklist.json" to json, "blocklist.json.sig" to sign(library, json),
            "index.json" to idx, "index.json.sig" to sign(library, idx)))
        val settings = AppSettings(context).apply { packWizardDone = true }
        val shared = SharedViewModel(dir, projectKey, server, settings)
        val work = tmp.newFolder("work")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(Section.LIBRARY), shared = shared) } }
        compose.onNodeWithTag("tab_shared").performClick()
        compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); compose.onAllNodes(hasTestTag("shared_withdrawn")).fetchSemanticsNodes().isNotEmpty() }
        assertFalse(bad.file.exists())
        assertTrue(good.file.exists())
        assertEquals(2L, settings.p2pListSequence)
        assertEquals(1L, settings.p2pIndexSequence)
        // The kept copies are read when the server is not reachable (rule 6).
        assertTrue(File(dir, "blocklist.json").isFile && File(dir, "index.json").isFile)
        // Sharing is off until the person turns it on; the reason is shown.
        compose.onNodeWithTag("shared_why").assertExists()
        assertEquals(SharingPolicy.Why.OFF, shared.state.value.blockedBy)
        // The index's model, and «Denunciar».
        compose.onNodeWithTag("shared_report_m1").performScrollTo().performClick()
        compose.onNodeWithTag("shared_report_reason_${R.string.report_rights}").performScrollTo().performClick()
        compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); compose.onAllNodes(hasTestTag("shared_reported")).fetchSemanticsNodes().isNotEmpty() }
        assertEquals("m1", server.reports.single().first)
        compose.onNodeWithTag("shared_code_m1").assertExists()
    }

    @Test
    fun `rule 6 without the server the copies kept from last time are used, and a replayed list is refused`() {
        val dir = tmp.newFolder("p2p")
        val json = list(9)
        File(dir, "key").writeBytes(keyJson())
        File(dir, "blocklist.json").writeBytes(json)
        File(dir, "blocklist.json.sig").writeBytes(sign(library, json))
        val shared = SharedViewModel(dir, projectKey, server = null)
        shared.refresh()
        val deadline = System.currentTimeMillis() + 10_000
        while (shared.state.value.list == null && System.currentTimeMillis() < deadline) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
        assertEquals(9L, shared.state.value.list?.sequence)
        assertNull(shared.state.value.problem)
    }

    @Test
    fun `a bad list and a failed report are said in the person's language, even without a message`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val json = list(1)
        val server = object : LibraryServer {
            override fun file(name: String) = when (name) {
                "key" -> keyJson(); "blocklist.json" -> json; "blocklist.json.sig" -> sign(pair(), json)
                else -> throw java.io.IOException("404")
            }
            override fun report(model: String?, sha256: String?, reason: String): String = throw java.io.IOException()
        }
        val settings = AppSettings(context).apply { packWizardDone = true }
        val shared = SharedViewModel(tmp.newFolder("p2p"), projectKey, server, settings)
        val work = tmp.newFolder("work")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(Section.LIBRARY), shared = shared) } }
        compose.onNodeWithTag("tab_shared").performClick()
        compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); compose.onAllNodes(hasTestTag("shared_problem")).fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithText(context.getString(R.string.shared_problem)).assertExists()
        assertTrue(compose.onAllNodes(hasText("signature", substring = true)).fetchSemanticsNodes().isEmpty())
        compose.runOnIdle { shared.report("m1", null, "rights") }
        compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); compose.onAllNodes(hasTestTag("shared_report_failed")).fetchSemanticsNodes().isNotEmpty() }
    }

    @Test
    fun `a network lost between a list and its signature keeps the good kept pair`() {
        val dir = tmp.newFolder("p2p")
        val old = list(4)
        File(dir, "key").writeBytes(keyJson())
        File(dir, "blocklist.json").writeBytes(old)
        File(dir, "blocklist.json.sig").writeBytes(sign(library, old))
        val fresh = list(5)
        // The new list arrives, its signature does not.
        val server = FakeServer(mapOf("key" to keyJson(), "blocklist.json" to fresh))
        val shared = SharedViewModel(dir, projectKey, server)
        shared.refresh()
        val deadline = System.currentTimeMillis() + 10_000
        while (shared.state.value.list == null && shared.state.value.problem == null && System.currentTimeMillis() < deadline) {
            ShadowLooper.idleMainLooper(); Thread.sleep(10)
        }
        assertNull(shared.state.value.problem)
        assertEquals(4L, shared.state.value.list?.sequence)
    }
}
