package net.tisma.app

import android.content.Context
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.p2p.BlockList
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
import java.security.KeyPairGenerator
import java.security.Signature
import java.time.Duration
import java.time.Instant
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** p2p_library.md: the block list, signed, applied to the phone's shared copies, and when the phone may share. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35], qualifiers = "w411dp-h891dp-xxhdpi")
class BlockListTest {
    @get:Rule
    val compose = createComposeRule()

    @get:Rule
    val tmp = TemporaryFolder()

    private val keys = KeyPairGenerator.getInstance("Ed25519").generateKeyPair()
    /** The raw 32-byte public key (the end of its X.509 encoding). */
    private val publicKey = keys.public.encoded.takeLast(32).toByteArray()

    private fun sign(b: ByteArray): ByteArray = Signature.getInstance("Ed25519").run { initSign(keys.private); update(b); sign() }

    private fun list(issued: Instant, hashes: List<String> = emptyList(), ipUntil: Instant? = null) = """{"format":"tisma-blocklist","version":1,
        "issued":"$issued","hashes":${hashes.joinToString(",", "[", "]") { "\"$it\"" }},"accounts":["acct-9"],"peers":["peer-1"],
        "ips":[${ipUntil?.let { "{\"ip\":\"203.0.113.7\",\"until\":\"$it\"}" } ?: ""}]}""".toByteArray()

    @Test
    fun `P2P5 a list is used only with a valid signature`() {
        val json = list(Instant.now())
        val l = BlockList.read(json, sign(json), publicKey)
        assertTrue(l.blocksAccount("acct-9"))
        assertTrue(l.blocksPeer("peer-1", null))
        val changed = list(Instant.now(), listOf("ab"))
        assertFailsWith<BlockList.BadList> { BlockList.read(changed, sign(json), publicKey) }
        // No key yet (the owner's): never accepted.
        assertFailsWith<BlockList.BadList> { BlockList.read(json, sign(json), ByteArray(0)) }
    }

    @Test
    fun `rule 5 an IP block lasts only its time, and P2P7 an old list stops sharing`() {
        val now = Instant.now()
        val json = list(now, ipUntil = now.plus(Duration.ofHours(24)))
        val l = BlockList.read(json, sign(json), publicKey, now)
        assertTrue(l.blocksPeer("other", "203.0.113.7", now))
        assertFalse(l.blocksPeer("other", "203.0.113.7", now.plus(Duration.ofHours(25))))
        val old = list(now.minus(Duration.ofDays(8)))
        val stale = BlockList.read(old, sign(old), publicKey, now)
        assertEquals(SharingPolicy.Why.STALE_LIST, SharingPolicy.blocked(true, stale, unmeteredNetwork = true, charging = true, now))
        // P2P6: on mobile data, no sharing; on Wi-Fi and charging, yes.
        assertEquals(SharingPolicy.Why.NOT_WIFI, SharingPolicy.blocked(true, l, unmeteredNetwork = false, charging = true, now))
        assertEquals(SharingPolicy.Why.NOT_CHARGING, SharingPolicy.blocked(true, l, unmeteredNetwork = true, charging = false, now))
        assertNull(SharingPolicy.blocked(true, l, unmeteredNetwork = true, charging = true, now))
        assertEquals(SharingPolicy.Why.NO_LIST, SharingPolicy.blocked(true, null, true, true, now))
    }

    @Test
    fun `P2P3 a withdrawn file is deleted from the phone and said`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val dir = tmp.newFolder("p2p")
        val copies = SharedCopies(File(dir, "files"))
        val bad = copies.add(tmp.newFile("bad.stl").apply { writeText("solid bad\nendsolid bad\n") }, "stl")
        val good = copies.add(tmp.newFile("good.stl").apply { writeText("solid good\nendsolid good\n") }, "stl")
        val json = list(Instant.now(), listOf(bad.sha256.uppercase()))
        val settings = AppSettings(context).apply { packWizardDone = true }
        val shared = SharedViewModel(dir, publicKey, fetch = { json to sign(json) }, saved = settings)
        val work = tmp.newFolder("work")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = settings)
        val cloud = CloudViewModel(settings, KeystoreSecretStore(context) { testKey }, "Test phone (Android)", pollMs = 5)
        val printers = PrintersViewModel(PrinterStore(context, MemorySecrets()))
        compose.setContent { TismaTheme { TismaApp(cloud, printers, slice, settings, Navigator(Section.LIBRARY), shared = shared) } }
        compose.onNodeWithTag("tab_shared").performClick()
        compose.waitUntil(10_000) { ShadowLooper.idleMainLooper(); compose.onAllNodes(hasTestTag("shared_withdrawn")).fetchSemanticsNodes().isNotEmpty() }
        assertFalse(bad.file.exists())
        assertTrue(good.file.exists())
        // Sharing is off until the person turns it on; the reason is shown.
        compose.onNodeWithTag("shared_why").assertExists()
        assertEquals(SharingPolicy.Why.OFF, shared.state.value.blockedBy)
    }
}
