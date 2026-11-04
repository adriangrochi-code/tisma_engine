package net.tisma.app

import android.content.Context
import android.graphics.Bitmap
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.data.AppSettings
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.ProfilePack
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.ui.BedTexture
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.GraphicsMode
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** The pack printer's bed in the 3D view (profile_pack.md, rule 14), with made-up files in the pack's layout. */
@RunWith(RobolectricTestRunner::class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
@Config(sdk = [35])
class BedTest {
    @get:Rule
    val tmp = TemporaryFolder()

    @Test
    fun `an SVG and a PNG picture become bitmaps of at most 1024 px`() {
        val svg = tmp.newFile("bed.svg").apply {
            writeText("""<svg width="100%" height="100%" viewBox="0 0 400 200" xmlns="http://www.w3.org/2000/svg">
                <rect x="0" y="0" width="200" height="200" fill="#ff0000"/></svg>""")
        }
        val b = assertNotNull(BedTexture.load(svg))
        assertEquals(1024, b.width); assertEquals(512, b.height)
        // The left half is the red square, the right half is empty.
        assertEquals(0xFFFF0000.toInt(), b.getPixel(100, 256))
        assertEquals(0, b.getPixel(900, 256) ushr 24)
        val png = tmp.newFile("bed.png")
        Bitmap.createBitmap(3000, 1500, Bitmap.Config.ARGB_8888).compress(Bitmap.CompressFormat.PNG, 100, png.outputStream())
        val p = assertNotNull(BedTexture.load(png))
        assertTrue(maxOf(p.width, p.height) <= 1024)
    }

    @Test
    fun `rule 14 the picture fills machine bed keeping its proportion, centred`() {
        val bed = floatArrayOf(235f, 235f)
        val wide = net.tisma.app.ui.fitRect(2f, bed)
        assertEquals(listOf(0f, 58.75f, 235f, 176.25f), wide.toList())
        val tall = net.tisma.app.ui.fitRect(0.5f, floatArrayOf(300f, 200f))
        assertEquals(listOf(100f, 0f, 200f, 200f), tall.toList())
    }

    @Test
    fun `rule 14 the bed model is centred on the bed and keeps its height`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        val data = tmp.newFolder("profiles")
        val pack = File(data, "pack").apply { mkdirs() }
        File(pack, "index.json").writeText("""{"pack_version":"1","vendors":[{"id":"Acme","name":"Acme","models":[{"name":"Acme One",
            "file":"vendors/Acme/models/One.json","variants":[{"id":"0.4","nozzle":0.4}],"default_variant":"0.4",
            "assets":{"bed_model":"vendors/Acme/assets/bed.stl","bed_texture":"vendors/Acme/assets/bed.svg"}}]}]}""")
        File(pack, "vendors/Acme/assets").mkdirs()
        // A 100 × 60 × 2 mm plate centred on its origin, its top at z = 0.
        File(pack, "vendors/Acme/assets/bed.stl").writeText(buildString {
            appendLine("solid b")
            for (t in listOf(listOf(-50f, -30f, 0f, 50f, -30f, 0f, 50f, 30f, 0f), listOf(-50f, -30f, -2f, 50f, 30f, -2f, -50f, 30f, -2f))) {
                appendLine("facet normal 0 0 1\nouter loop")
                for (k in 0..2) appendLine("vertex ${t[3 * k]} ${t[3 * k + 1]} ${t[3 * k + 2]}")
                appendLine("endloop\nendfacet")
            }
            appendLine("endsolid b")
        })
        File(pack, "vendors/Acme/assets/bed.svg").writeText("""<svg viewBox="0 0 10 10" xmlns="http://www.w3.org/2000/svg"/>""")
        val work = tmp.newFolder("work")
        val exe = File(work, "fake").apply { writeText("#!/bin/sh\nexit 1\n"); setExecutable(true) }
        val slice = SliceViewModel(EngineRunner(exe, work, data), work, saved = AppSettings(context), pack = ProfilePack(exe, data))
        slice.setProfile(EngineRunner.ProfileChoice("vendors/Acme/models/One.json#0.4", 0.4, null, pack = true, name = "Acme One"))
        eventually { org.robolectric.shadows.ShadowLooper.idleMainLooper(); slice.state.value.bedModel != null }
        val s = slice.state.value
        val b = s.bedModel!!.bounds
        assertEquals(s.bed[0] / 2, (b[0] + b[3]) / 2, 1e-3f)
        assertEquals(s.bed[1] / 2, (b[1] + b[4]) / 2, 1e-3f)
        assertEquals(-0.05f, b[5], 1e-4f)
        assertEquals("bed.svg", s.bedTexture!!.name)
    }

    /** With an installed pack (TISMA_PACK_INSTALLED, its `pack` folder): every bed picture of it reads. */
    @Test
    fun `the pack's own bed pictures read`() {
        val dir = System.getenv("TISMA_PACK_INSTALLED")?.let(::File)
        org.junit.Assume.assumeTrue(dir != null && dir.isDirectory)
        val pics = dir!!.walkTopDown().filter { it.isFile && it.parentFile.name == "assets" && it.extension.lowercase() in setOf("svg", "png")
            && !it.name.contains("thumbnail") }.toList()
        val bad = pics.filter { BedTexture.load(it) == null }
        println("bed pictures ${pics.size}, unreadable ${bad.map { it.name }}")
        assertTrue(pics.isNotEmpty() && bad.isEmpty())
    }
}
