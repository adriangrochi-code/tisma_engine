package net.tisma.app

import net.tisma.app.engine.fold
import net.tisma.app.engine.searchWords
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Searches find words typed without accents (help, settings, printers, My library). */
class FoldTest {
    @Test
    fun `accents and case do not matter`() {
        assertTrue(fold("Deformación por calor (warping)").contains("deformacion"))
        assertTrue(fold("Retracción").contains(fold("retraccion")))
        assertEquals(listOf("boquilla", "endurecida"), searchWords("  Boquílla   ENDURECIDA "))
        // Both sides fold the same way, so «ñ» typed or not finds it.
        assertEquals(fold("ñandu"), fold("Ñandú"))
    }

    @Test
    fun `the printer search ignores accents`() {
        val root = java.io.File(System.getProperty("user.dir")).parentFile.parentFile.parentFile
        val c = net.tisma.app.engine.Catalog.parse(java.io.File(root, "resources/catalog/printers.json").readText(),
            java.io.File(root, "resources/catalog/filaments.json").readText())
        assertEquals(c.search("ankermake m5").map { it.name }, c.search("ÁnkerMake M5").map { it.name })
    }
}
