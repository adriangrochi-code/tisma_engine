package net.tisma.app.p2p

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.time.Clock
import kotlin.time.Duration.Companion.days
import kotlinx.serialization.json.JsonPrimitive

/** The parts of p2p_library.md that run the same on Android and iOS. */
class SharedLogicTest {
    @Test
    fun keysAreReadInHexOrBase64() {
        assertContentEquals(byteArrayOf(1, 2, -1), LibraryKey.bytes(JsonPrimitive("0102ff")))
        assertContentEquals(byteArrayOf(1, 2, -1), LibraryKey.bytes(JsonPrimitive("AQL/")))
        assertNull(LibraryKey.bytes(JsonPrimitive("  ")))
        assertEquals("0102ff", LibraryKey.hex(byteArrayOf(1, 2, -1)))
    }

    @Test
    fun sharingWaitsForAFreshListWiFiAndCharger() {
        val now = Clock.System.now()
        val list = BlockList(now, 1, now + 7.days, emptyMap(), emptySet(), emptySet(), emptySet())
        assertEquals(SharingPolicy.Why.OFF, SharingPolicy.blocked(false, list, true, true, now))
        assertEquals(SharingPolicy.Why.STALE_LIST, SharingPolicy.blocked(true, list, true, true, now + 8.days))
        assertEquals(SharingPolicy.Why.NOT_WIFI, SharingPolicy.blocked(true, list, false, true, now))
        assertNull(SharingPolicy.blocked(true, list, true, true, now))
    }

    @Test
    fun theIndexSearchLeavesOutGcodeAndBlockedAuthors() {
        val now = Clock.System.now()
        fun m(id: String, account: String, path: String) = LibraryIndex.Model(id, 1, "Gear $id", "", listOf("gear"), "CC0", "Ana",
            account, 1, listOf(LibraryIndex.FileRef(path, "model", 1, id.padEnd(64, '0'))), false, "")
        val index = LibraryIndex(1, listOf(m("a", "acct-1", "x.3mf"), m("b", "acct-9", "y.3mf"), m("c", "acct-1", "z.gcode")))
        val list = BlockList(now, 1, now + 7.days, emptyMap(), setOf("acct-9"), emptySet(), emptySet())
        assertEquals(listOf("a"), index.search("gear", list).map { it.id })
    }
}
