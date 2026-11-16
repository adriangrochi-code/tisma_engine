package net.tisma.app

import android.app.Activity
import android.app.Instrumentation
import android.content.Intent
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.semantics.getOrNull
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTextInput
import androidx.core.content.FileProvider
import androidx.test.core.app.ActivityScenario
import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.IntentMatchers.hasAction
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.doubleOrNull
import net.tisma.app.engine.GcodeLayers
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.security.MessageDigest
import kotlin.math.abs
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.test.fail

/**
 * The release gate's G16 for the Android app (docs/spec/RELEASE_GATE.md, owner's decision 16-11-2026): on an
 * emulator, through the app's own screens, the first-use assistant, opening (G1), slicing (G2) and exporting and
 * sending (G10), with each result checked — the part's size on the plate, the G-code's layers, filament, time and
 * dimensions, the exported file, and the file each simulated printer received and started.
 *
 * Run by the workflow «Release gate (phones)» after `pm clear`, with the printer simulators on the host
 * (`:mockprinters:run`, 10.0.2.2:7125–7128) and the 3DBenchy at the path in the argument `benchy`. Nothing here is
 * skipped or relaxed (RELEASE_GATE.md, rule 3).
 */
@RunWith(AndroidJUnit4::class)
class ReleaseGateTest {
    @get:Rule
    val compose = createEmptyComposeRule()

    private val instrumentation = InstrumentationRegistry.getInstrumentation()
    private val context = instrumentation.targetContext
    private val share = File(context.cacheDir, "share/gate").apply { mkdirs() }
    private val work = File(context.cacheDir, "engine")
    private var scenario: ActivityScenario<MainActivity>? = null

    @Before
    fun intents() = Intents.init()

    @After
    fun close() {
        scenario?.close()
        Intents.release()
    }

    @Test
    fun g16_assistant_open_slice_export_and_send() {
        assistant()
        // G1: each kind of file, opened as another app hands it over («Open with»), is on the plate with its size.
        open(cube20(), "20.0 × 20.0 × 20.0 mm")
        open(asset("box.obj"), "20.0 × 20.0 × 10.0 mm")
        open(asset("box.3mf"), "20.0 × 20.0 × 10.0 mm")
        open(asset("pi2_prusaslicer.3mf"), "30.0 × 10.0 × 15.0 mm")
        // G2: the Benchy, then the 20 mm cube (left on the plate for G10).
        val benchy = benchy()
        open(benchy, sizeText(stlBounds(benchy)))
        checkGcode(slice("3DBenchy"), stlBounds(benchy))
        open(cube20(), "20.0 × 20.0 × 20.0 mm")
        val gcode = slice("cube20")
        checkGcode(gcode, floatArrayOf(0f, 0f, 0f, 20f, 20f, 20f))
        // G10: exported through the system's «save as», and sent to each simulated printer, where it prints.
        export(gcode)
        send(gcode)
    }

    /** MA1: the first start opens the assistant; with the built-in catalog it ends with a printer chosen. */
    private fun assistant() {
        scenario = ActivityScenario.launch(MainActivity::class.java)
        waitTag("pack_wizard", 60_000)
        tap("pack_builtin")
        waitTag("profile_search")
        val printer = firstTagStarting("profile_printer_")
        tap(printer)
        val filament = firstTagStarting("profile_filament_")
        tap(filament)
        tap("profile_save")
        waitTag("plate_slice")
    }

    /** «Open with» Tisma: the file as a content address, as a file manager or a browser hands it over. */
    private fun open(file: File, size: String) {
        scenario?.close()
        val uri = FileProvider.getUriForFile(context, context.packageName + ".updates", file)
        val intent = Intent(Intent.ACTION_VIEW).setClass(context, MainActivity::class.java)
            .setDataAndType(uri, "application/octet-stream").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        scenario = ActivityScenario.launch(intent)
        // The plate names this file (a new start first restores the last model), then its size is read.
        // The name shows first; a large model's mesh (and so its size) comes a moment later: wait for this file's size.
        runCatching { compose.waitUntil(60_000) { text("plate_model")?.contains(file.name) == true && text("plate_info")?.startsWith(size) == true } }
        assertTrue(text("plate_model")?.contains(file.name) == true, "${file.name} is on the plate (it shows «${text("plate_model")}»)")
        // «20.0 × 20.0 × 20.0 mm · 12 triangles», for a model and for a project.
        assertEquals(size, text("plate_info")?.substringBefore(" ·"), "the size of ${file.name} on the plate")
    }

    /** Slices on the phone from the plate's button; the G-code it made. */
    private fun slice(name: String): File {
        tap("plate_slice")
        compose.waitUntil(15 * 60_000) { has("plate_result") || has("plate_error") }
        if (has("plate_error")) fail("slicing $name: ${text("plate_error")}")
        assertTrue(!has("profile_not_used"), "the chosen profile was used for $name")
        val gcode = File(work, "$name.gcode")
        assertTrue(gcode.isFile && gcode.length() > 0, "the G-code of $name exists")
        val summary = Json.parseToJsonElement(File(work, "summary.json").readText()) as JsonObject
        for (k in listOf("layers", "filament_mm", "print_time_s")) {
            val v = (summary[k] as? JsonPrimitive)?.doubleOrNull ?: 0.0
            assertTrue(v > 0, "$name: $k > 0 (was $v)")
        }
        return gcode
    }

    private val PART = listOf("perimeter", "wall", "infill", "bridge", "gap", "overhang", "ironing", "top", "bottom")

    /** G2: the printed part (walls and infill, not skirt or support) is the model's size within 0.1 mm. */
    private fun checkGcode(gcode: File, model: FloatArray) {
        val layers = gcode.bufferedReader().useLines { GcodeLayers.read(it) }
        var minX = Float.MAX_VALUE; var minY = Float.MAX_VALUE; var maxX = -Float.MAX_VALUE; var maxY = -Float.MAX_VALUE
        var top = 0f
        for (l in layers.layers) for (i in 0 until l.count) {
            val type = layers.types.getOrElse(l.segmentTypes[i].toInt()) { "" }.lowercase()
            // Only the part: walls, infill, bridges, gap fill, ironing. Not the skirt or brim, support, wipe tower, nor
            // the start G-code's purge line (untyped, «Custom»).
            if (PART.none { it in type } || "support" in type) continue
            val half = l.widths[i] / 2
            val s = l.segments
            for (k in 0..1) {
                val x = s[4 * i + 2 * k]; val y = s[4 * i + 2 * k + 1]
                minX = minOf(minX, x - half); maxX = maxOf(maxX, x + half); minY = minOf(minY, y - half); maxY = maxOf(maxY, y + half)
            }
            top = maxOf(top, l.z)
        }
        val want = floatArrayOf(model[3] - model[0], model[4] - model[1], model[5] - model[2])
        val got = floatArrayOf(maxX - minX, maxY - minY, top)
        for (a in 0..2) assertTrue(abs(got[a] - want[a]) <= 0.1f, "${gcode.name}: ${"XYZ"[a]} is ${got[a]} mm, the model ${want[a]} mm")
    }

    /** G10, export: «Save G-code» goes to the system's «save as»; here it answers with a file of the app's own. */
    private fun export(gcode: File) {
        val target = File(share, "exported.gcode").apply { delete(); createNewFile() }
        val uri = FileProvider.getUriForFile(context, context.packageName + ".updates", target)
        Intents.intending(hasAction(Intent.ACTION_CREATE_DOCUMENT))
            .respondWith(Instrumentation.ActivityResult(Activity.RESULT_OK, Intent().setData(uri)
                .addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION)))
        tap("plate_save")
        compose.waitUntil(30_000) { target.length() == gcode.length() }
        assertTrue(sha(target.readBytes()) == sha(gcode.readBytes()), "the exported G-code is the sliced one, whole")
    }

    /** G10, send: each kind of simulated printer is added on the Printer screen and gets the G-code, which starts. */
    private fun send(gcode: File) {
        val printers = listOf(
            Triple("moonraker", "http://10.0.2.2:7125", ""),
            Triple("octoprint", "http://10.0.2.2:7126", "test-key"),
            Triple("prusalink", "http://10.0.2.2:7127", "test-pass"),
            Triple("duet", "http://10.0.2.2:7128", "reprap"),
        )
        for ((kind, url, key) in printers) {
            compose.onNodeWithTag("section_printer").performClick()
            waitTag("add_printer")
            compose.onNodeWithTag("add_printer").performClick()
            waitTag("printer_address")
            compose.onNodeWithTag("kind_$kind").performClick()
            compose.onNodeWithTag("printer_name").performTextInput("gate-$kind")
            compose.onNodeWithTag("printer_address").performTextInput(url)
            if (key.isNotEmpty()) compose.onNodeWithTag("printer_key").performTextInput(key)
            tap("printer_save")
            compose.waitUntil(60_000) { !has("printer_save") }
        }
        val want = sha(gcode.readBytes())
        for ((kind, url, _) in printers) {
            compose.onNodeWithTag("section_plate").performClick()
            waitTag("send_to_gate-$kind")
            tap("send_to_gate-$kind")
            tap("plate_send")
            try {
                compose.waitUntil(120_000) { received(url, want) }
            } catch (e: Throwable) {
                val said = runCatching { java.net.URL("$url/_tisma/received").readText() }.getOrElse { "(no answer: ${it.message})" }
                fail("$kind did not get the G-code (sha256 $want, ${gcode.length()} bytes) or did not start; it says $said; on screen: ${screen()}")
            }
        }
    }

    /** The simulator's account of what arrived (`/_tisma/received`): the whole G-code, and the print started. */
    private fun received(url: String, sha: String): Boolean = runCatching {
        val o = Json.parseToJsonElement(java.net.URL("$url/_tisma/received").readText()) as JsonObject
        val files = o["files"] as JsonObject
        val arrived = files.values.any { ((it as JsonObject)["sha256"] as JsonPrimitive).content == sha }
        val state = (o["state"] as JsonPrimitive).content
        arrived && state in setOf("printing", "complete")
    }.getOrDefault(false)

    private fun cube20(): File = File(share, "cube20.stl").apply {
        val v = listOf(0f, 20f)
        val faces = listOf(
            // Two triangles per side of the 20 mm cube, outward.
            listOf(0, 2, 3, 0, 3, 1), listOf(4, 5, 7, 4, 7, 6), listOf(0, 1, 5, 0, 5, 4),
            listOf(2, 6, 7, 2, 7, 3), listOf(0, 4, 6, 0, 6, 2), listOf(1, 3, 7, 1, 7, 5))
        fun p(i: Int) = "${v[(i shr 0) and 1]} ${v[(i shr 1) and 1]} ${v[(i shr 2) and 1]}"
        writeText(buildString {
            append("solid cube20\n")
            for (f in faces) for (t in 0..1) {
                append(" facet normal 0 0 0\n  outer loop\n")
                for (k in 0..2) append("   vertex ${p(f[3 * t + k])}\n")
                append("  endloop\n endfacet\n")
            }
            append("endsolid cube20\n")
        })
    }

    private fun asset(name: String): File = File(share, name).apply {
        InstrumentationRegistry.getInstrumentation().context.assets.open(name).use { i -> outputStream().use { i.copyTo(it) } }
    }

    private fun benchy(): File {
        val from = InstrumentationRegistry.getArguments().getString("benchy") ?: fail("no 3DBenchy given (argument benchy)")
        return File(share, "3DBenchy.stl").apply { File(from).inputStream().use { i -> outputStream().use { i.copyTo(it) } } }
    }

    /** Binary STL bounds: min x y z, max x y z. */
    private fun stlBounds(f: File): FloatArray {
        val b = java.nio.ByteBuffer.wrap(f.readBytes()).order(java.nio.ByteOrder.LITTLE_ENDIAN)
        val n = b.getInt(80)
        val r = floatArrayOf(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE)
        for (t in 0 until n) for (k in 0..2) for (a in 0..2) {
            val x = b.getFloat(84 + 50 * t + 12 + 12 * k + 4 * a)
            r[a] = minOf(r[a], x); r[3 + a] = maxOf(r[3 + a], x)
        }
        return r
    }

    private fun sizeText(b: FloatArray) = String.format(java.util.Locale.getDefault(), "%.1f × %.1f × %.1f mm", b[3] - b[0], b[4] - b[1], b[5] - b[2])

    private fun sha(b: ByteArray) = MessageDigest.getInstance("SHA-256").digest(b).joinToString("") { "%02x".format(it) }

    /** Taps [tag]; scrolled into view first when it sits in a scrolling screen (not every screen scrolls). */
    private fun tap(tag: String) {
        val node = compose.onNodeWithTag(tag)
        runCatching { node.performScrollTo() }
        node.performClick()
    }

    private fun has(tag: String) = compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty()

    private fun text(tag: String): String? = compose.onAllNodes(hasTestTag(tag), useUnmergedTree = true).fetchSemanticsNodes()
        .firstOrNull()?.config?.getOrNull(SemanticsProperties.Text)?.joinToString("") { it.text }

    private fun waitTag(tag: String, ms: Long = 30_000) {
        try { compose.waitUntil(ms) { has(tag) } } catch (e: Throwable) { fail("«$tag» not shown after $ms ms; on screen: ${screen()}") }
    }

    /** What the screen holds now (tags and texts), for a failure's message. */
    private fun screen(): String = runCatching {
        compose.onAllNodes(SemanticsMatcher("any") { true }, useUnmergedTree = true).fetchSemanticsNodes().mapNotNull { n ->
            val tag = n.config.getOrNull(SemanticsProperties.TestTag)
            val text = n.config.getOrNull(SemanticsProperties.Text)?.joinToString("") { it.text }
            listOfNotNull(tag?.let { "#$it" }, text?.take(40)).joinToString(" ").ifBlank { null }
        }.distinct().take(60).joinToString(" | ")
    }.getOrElse { "(no screen: ${it.message})" }

    private fun firstTagStarting(prefix: String): String {
        val m = SemanticsMatcher("tag starts with $prefix") { (it.config.getOrNull(SemanticsProperties.TestTag) ?: "").startsWith(prefix) }
        compose.waitUntil(30_000) { compose.onAllNodes(m, useUnmergedTree = true).fetchSemanticsNodes().isNotEmpty() }
        return compose.onAllNodes(m, useUnmergedTree = true).fetchSemanticsNodes().first().config[SemanticsProperties.TestTag]
    }
}
