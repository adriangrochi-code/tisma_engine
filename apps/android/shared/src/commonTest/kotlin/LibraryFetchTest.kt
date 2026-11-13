import net.tisma.app.p2p.LibraryFetch
import net.tisma.app.p2p.LibraryIndex
import net.tisma.app.p2p.LibraryServer
import net.tisma.app.p2p.LibraryStore
import net.tisma.app.p2p.LibraryKey
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** p2p_library.md, rule 6, the same on Android and iOS (JVM and iPhone simulator). */
class LibraryFetchTest {
    private class Memory(override var origin: String? = "") : LibraryStore {
        val files = mutableMapOf<String, ByteArray>()
        override fun read(name: String) = files[name]
        override fun write(name: String, bytes: ByteArray) { files[name] = bytes }
        override fun delete(name: String) { files.remove(name) }
        override var listSequence = -1L
        override var indexSequence = -1L
    }

    private fun server(origin: String, files: Map<String, ByteArray> = emptyMap()) = object : LibraryServer {
        override fun file(name: String) = files[name] ?: throw net.tisma.io.IOException("no answer")
        override fun report(model: String?, sha256: String?, reason: String) = ""
        override val origin = origin
    }

    @Test
    fun nothingAnywhereIsNothing() {
        val r = LibraryFetch(ByteArray(32), server("https://a"), Memory()).refresh()
        assertNull(r.list); assertNull(r.index)
    }

    @Test
    fun anotherServerForgetsTheKeptPiecesAndSequences() {
        val store = Memory("https://a").apply {
            write("key", "{}".encodeToByteArray()); write("index.json", "{}".encodeToByteArray())
            listSequence = 40; indexSequence = 41
        }
        LibraryFetch(ByteArray(32), server("https://b"), store).refresh()
        assertTrue(store.files.isEmpty())
        assertEquals(-1L, store.listSequence)
        assertEquals("https://b", store.origin)
    }

    @Test
    fun withoutTheProjectKeyAKeyIsRefusedAndNotKept() {
        val store = Memory()
        assertFailsWith<LibraryKey.BadKey> {
            LibraryFetch(ByteArray(0), server("https://a", mapOf("key" to """{"public_key":"","certificate":""}""".encodeToByteArray())), store).refresh()
        }
        assertNull(store.read("key"))
    }

    /** Rule 3 ter bis, on both phones: an index over the limit is refused as too large before anything else. */
    @Test
    fun anIndexOverTheLimitIsTooLarge() {
        assertFailsWith<LibraryIndex.TooLarge> { LibraryIndex.read(ByteArray(11), ByteArray(64), ByteArray(32), maxJson = 10) }
        assertEquals(64L * 1024 * 1024, LibraryIndex.MAX_COMPRESSED)
        assertEquals(256L * 1024 * 1024, LibraryIndex.MAX_JSON)
    }
}
