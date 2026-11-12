package net.tisma.app.engine

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** The iPhone's printer card takes its drawings from the same tables as Android's (settings_panel.md, rules 34 to 36). */
class PrinterCardLookTest {
    @Test
    fun anEnclosedBambuWithAHardenedNozzleAndItsPlate() {
        val l = EngineBridge.cardLook("""{"machine":{"enclosed":true,"hardened_nozzle":true},"bambu":{"plate":"Textured PEI Plate"}}""",
            "Bambu Lab X1 Carbon 0.4 HF")
        assertEquals("enclosed", l.shape)
        assertTrue(l.hardened && l.highFlow && l.bambu)
        assertEquals("textured", l.bed)
    }

    @Test
    fun aBrokenProfileIsAnOpenPrinter() {
        val l = EngineBridge.cardLook("not json", "Prusa MK4S")
        assertEquals("open", l.shape)
        assertEquals(false, l.bambu)
        assertEquals("other", l.bed)
    }
}

class EngineeringBridgeTest {
    @Test
    fun theCaseAndArgumentsAreAndroids() {
        val case = EngineeringBridge.caseJson("bottom", "top", 80.0, "bambu-petg-basic", "normal", false)
        assertTrue("\"face\":\"bottom\"" in case && "-80.0" in case && "bambu-petg-basic" in case)
        assertEquals(listOf("analyze", "m.stl", "--config", "c.json", "--case", "k.json", "-o", "r.json"),
            EngineeringBridge.analyzeArgs("m.stl", "c.json", "k.json", "r.json"))
        assertEquals(ReportLine("verdict", "holds"), EngineeringBridge.report("""{"valid":true,"result":{"min_safety":4}}""", 2.0).first())
    }
}
