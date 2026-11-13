package net.tisma.app.engine

import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import platform.Foundation.NSData
import platform.Foundation.create
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
        assertEquals(listOf(0.0, 0.0, 220.0, 220.0), l.bedArea)
        val mk4 = EngineBridge.cardLook("""{"machine":{"bed":[[0,-4],[250,-4],[250,210],[0,210]]}}""", "Prusa MK4S")
        assertEquals(listOf(0.0, -4.0, 250.0, 210.0), mk4.bedArea)
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
        assertEquals(listOf("analyze", "m.stl", "--config", "c.json", "--case", "k.json", "-o", "r.json", "--mesh", "v.ply"),
            EngineeringBridge.analyzeArgs("m.stl", "c.json", "k.json", "r.json", "v.ply"))
        assertEquals(ReportLine("verdict", "holds"), EngineeringBridge.report("""{"valid":true,"result":{"min_safety":4}}""", 2.0).first())
    }
}

@OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)
class EngineeringPickTest {
    @Test
    fun aTouchedTopIsTheEnginesTopAndPushesDown() {
        val s = 20f
        val v = listOf(floatArrayOf(0f, 0f, 0f), floatArrayOf(s, 0f, 0f), floatArrayOf(s, s, 0f), floatArrayOf(0f, s, 0f),
            floatArrayOf(0f, 0f, s), floatArrayOf(s, 0f, s), floatArrayOf(s, s, s), floatArrayOf(0f, s, s))
        val idx = intArrayOf(0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7)
        val soup = idx.flatMap { v[it].toList() }.toFloatArray()
        val data = soup.usePinned { p -> NSData.create(bytes = p.addressOf(0), length = (soup.size * 4).toULong()) }
        assertEquals(12, EngineeringBridge.setMesh(data))
        val top = EngineeringBridge.pick(2, "normal")!!
        assertEquals("top", top.face)
        assertEquals(setOf(2, 3), top.triangles.toSet())
        val case = EngineeringBridge.caseFor(EngineeringBridge.named("bottom"), top, 100.0, "bambu-pla-basic", "normal", true)
        assertTrue("\"face\":\"top\"" in case && "-100.0" in case, case)
    }
}

@OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)
class EngineeringExtrasTest {
    @Test
    fun aPointLoadAlongAnAxisAndTheTemperature() {
        val s = 20f
        val v = listOf(floatArrayOf(0f, 0f, 0f), floatArrayOf(s, 0f, 0f), floatArrayOf(s, s, 0f), floatArrayOf(0f, s, 0f),
            floatArrayOf(0f, 0f, s), floatArrayOf(s, 0f, s), floatArrayOf(s, s, s), floatArrayOf(0f, s, s))
        val idx = intArrayOf(0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7)
        val soup = idx.flatMap { v[it].toList() }.toFloatArray()
        EngineeringBridge.setMesh(soup.usePinned { p -> NSData.create(bytes = p.addressOf(0), length = (soup.size * 4).toULong()) })
        val p = EngineeringBridge.point(2, 10.0, 10.0, 20.0, 1.0)!!
        assertEquals(listOf(9.0, 9.0, 19.0, 11.0, 11.0, 21.0), p.box)
        val case = EngineeringBridge.caseAlong(EngineeringBridge.named("bottom"), p, 50.0, "+x", "bambu-pla-basic", "normal", true)
        assertTrue("\"box\"" in case && "[50.0,0.0,0.0]" in case.replace(" ", "") || "[50.0,-0.0,-0.0]" in case.replace(" ", ""), case)
        val hot = EngineeringBridge.withNozzleTemperature("""{"machine":{"variables":{"temperature":210}}}""", 230.0)
        assertTrue("230" in hot && "210" !in hot, hot)
    }
}

class LightestInfillTest {
    @Test
    fun theDensityOnlyWhenFound() {
        assertEquals(15, EngineeringBridge.lightestDensity("""{"found":true,"density":15.0}"""))
        assertEquals(-1, EngineeringBridge.lightestDensity("""{"found":true,"solid":true,"density":100}"""))
        assertEquals(-1, EngineeringBridge.lightestDensity("""{"found":false}"""))
        assertTrue("lightest-infill" in EngineeringBridge.lightestArgs("p.3mf", "r.tea", "s.json", ""))
        assertEquals(20, EngineeringBridge.lightestDensityInReport("""{"format":"tisma-engineering-result","summary":{"found":true,"density":20}}"""))
        assertTrue("--case" in EngineeringBridge.lightestModelArgs("m.stl", "c.json", "k.json", "r.json", 2.0))
    }
}
