package net.tisma.app.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlin.math.hypot

/** The printer's drawing, one per kind (settings_panel.md, rule 34). */
enum class PrinterShape { RESIN, PELLETS, BELT, DELTA, ENCLOSED, OPEN }

/** The bed's drawing, one per family of plate names (rule 35). */
enum class BedFamily { TEXTURED, SMOOTH, GLASS, COOL, ENGINEERING, OTHER }

/** Rule 34's table, in its order, from the profile's keys (docs/CONFIG.md). */
fun printerShape(value: (String) -> JsonElement?): PrinterShape {
    fun text(k: String) = (value(k) as? JsonPrimitive)?.content.orEmpty()
    fun yes(k: String) = (value(k) as? JsonPrimitive)?.booleanOrNull == true
    return when {
        text("machine.technology") == "msla" -> PrinterShape.RESIN
        text("machine.feedstock") == "pellet" -> PrinterShape.PELLETS
        yes("belt.enabled") -> PrinterShape.BELT
        roundBed(value("machine.bed")) -> PrinterShape.DELTA
        yes("machine.enclosed") -> PrinterShape.ENCLOSED
        else -> PrinterShape.OPEN
    }
}

/** A bed outline is a circle when it has many points, all at about the same distance from their centre. */
internal fun roundBed(bed: JsonElement?): Boolean {
    val pts = (bed as? JsonArray)?.mapNotNull { p ->
        val a = p as? JsonArray ?: return@mapNotNull null
        val x = (a.getOrNull(0) as? JsonPrimitive)?.doubleOrNull; val y = (a.getOrNull(1) as? JsonPrimitive)?.doubleOrNull
        if (x != null && y != null) x to y else null
    }.orEmpty()
    if (pts.size < 12) return false
    val cx = pts.sumOf { it.first } / pts.size; val cy = pts.sumOf { it.second } / pts.size
    val r = pts.map { hypot(it.first - cx, it.second - cy) }
    val mean = r.average()
    return mean > 0 && r.all { kotlin.math.abs(it - mean) < 0.05 * mean }
}

/** Rule 35's table, in its order, by what the plate's name contains (any case). */
fun bedFamily(name: String): BedFamily {
    val n = name.lowercase()
    return when {
        "textur" in n || "satin" in n -> BedFamily.TEXTURED
        "smooth" in n || "pei" in n -> BedFamily.SMOOTH
        "glass" in n || "vidrio" in n -> BedFamily.GLASS
        "cool" in n -> BedFamily.COOL
        "engineering" in n || "high temp" in n -> BedFamily.ENGINEERING
        else -> BedFamily.OTHER
    }
}

/** Rule 36: a high-flow variant says so in its name («high flow» or «HF»). */
fun highFlow(name: String): Boolean = Regex("""(?i)\bhigh[ -]?flow\b|\bHF\b""").containsMatchIn(name)

/** One-ink drawings in violet on the soft violet background (interface.md, «Aspecto»), in both themes. */
@Composable
private fun Plate(size: Dp, tag: String, draw: androidx.compose.ui.graphics.drawscope.DrawScope.(ink: androidx.compose.ui.graphics.Color) -> Unit) {
    val ink = MaterialTheme.colorScheme.primary
    Canvas(Modifier.size(size).background(MaterialTheme.colorScheme.primaryContainer, RoundedCornerShape(8.dp)).testTag(tag)) { draw(ink) }
}

@Composable
fun PrinterDrawing(shape: PrinterShape, size: Dp = 48.dp) = Plate(size, "drawing_printer_" + shape.name.lowercase()) { ink ->
    val w = this.size.width; val h = this.size.height; val s = Stroke(w * 0.05f)
    when (shape) {
        PrinterShape.OPEN -> {
            // Two uprights, the gantry across, a nozzle under it, the bed below.
            drawLine(ink, Offset(w * .2f, h * .2f), Offset(w * .2f, h * .8f), s.width)
            drawLine(ink, Offset(w * .8f, h * .2f), Offset(w * .8f, h * .8f), s.width)
            drawLine(ink, Offset(w * .2f, h * .3f), Offset(w * .8f, h * .3f), s.width)
            drawRect(ink, Offset(w * .44f, h * .3f), Size(w * .12f, h * .14f))
            drawLine(ink, Offset(w * .15f, h * .8f), Offset(w * .85f, h * .8f), s.width * 1.4f)
        }
        PrinterShape.ENCLOSED -> {
            drawRoundRect(ink, Offset(w * .15f, h * .15f), Size(w * .7f, h * .7f), CornerRadius(w * .06f), style = s)
            drawRect(ink, Offset(w * .44f, h * .25f), Size(w * .12f, h * .14f))
            drawLine(ink, Offset(w * .25f, h * .7f), Offset(w * .75f, h * .7f), s.width)
        }
        PrinterShape.DELTA -> {
            drawLine(ink, Offset(w * .25f, h * .15f), Offset(w * .25f, h * .85f), s.width)
            drawLine(ink, Offset(w * .75f, h * .15f), Offset(w * .75f, h * .85f), s.width)
            drawLine(ink, Offset(w * .25f, h * .3f), Offset(w * .5f, h * .5f), s.width * .6f)
            drawLine(ink, Offset(w * .75f, h * .3f), Offset(w * .5f, h * .5f), s.width * .6f)
            drawOval(ink, Offset(w * .3f, h * .74f), Size(w * .4f, h * .1f), style = s)
        }
        PrinterShape.BELT -> {
            drawRoundRect(ink, Offset(w * .1f, h * .68f), Size(w * .8f, h * .14f), CornerRadius(h * .07f), style = s)
            drawLine(ink, Offset(w * .3f, h * .2f), Offset(w * .6f, h * .62f), s.width)
            drawRect(ink, Offset(w * .55f, h * .5f), Size(w * .1f, h * .12f))
        }
        PrinterShape.RESIN -> {
            drawRect(ink, Offset(w * .2f, h * .55f), Size(w * .6f, h * .3f), style = s)
            drawRoundRect(ink, Offset(w * .3f, h * .15f), Size(w * .4f, h * .4f), CornerRadius(w * .05f), style = s)
            drawLine(ink, Offset(w * .5f, h * .1f), Offset(w * .5f, h * .3f), s.width)
        }
        PrinterShape.PELLETS -> {
            val hopper = Path().apply { moveTo(w * .3f, h * .15f); lineTo(w * .7f, h * .15f); lineTo(w * .55f, h * .45f); lineTo(w * .45f, h * .45f); close() }
            drawPath(hopper, ink, style = s)
            drawRect(ink, Offset(w * .44f, h * .45f), Size(w * .12f, h * .2f))
            drawLine(ink, Offset(w * .15f, h * .8f), Offset(w * .85f, h * .8f), s.width * 1.4f)
        }
    }
}

@Composable
fun BedDrawing(family: BedFamily, size: Dp = 32.dp) = Plate(size, "drawing_bed_" + family.name.lowercase()) { ink ->
    val w = this.size.width; val h = this.size.height
    val top = Offset(w * .12f, h * .35f); val plate = Size(w * .76f, h * .3f)
    drawRoundRect(ink, top, plate, CornerRadius(w * .04f), style = Stroke(w * .05f))
    when (family) {
        BedFamily.TEXTURED -> for (i in 0..4) for (j in 0..1) drawCircle(ink, w * .025f, Offset(w * (.22f + i * .14f), h * (.44f + j * .12f)))
        BedFamily.SMOOTH -> drawLine(ink.copy(alpha = .5f), Offset(w * .2f, h * .45f), Offset(w * .5f, h * .45f), w * .04f)
        BedFamily.GLASS -> drawLine(ink, Offset(w * .3f, h * .6f), Offset(w * .5f, h * .4f), w * .04f)
        BedFamily.COOL -> {
            val c = Offset(w * .5f, h * .5f); val r = w * .1f
            for (a in 0 until 3) {
                val t = a * Math.PI / 3
                drawLine(ink, Offset(c.x - r * kotlin.math.cos(t).toFloat(), c.y - r * kotlin.math.sin(t).toFloat()),
                    Offset(c.x + r * kotlin.math.cos(t).toFloat(), c.y + r * kotlin.math.sin(t).toFloat()), w * .03f)
            }
        }
        BedFamily.ENGINEERING -> {
            val flame = Path().apply { moveTo(w * .5f, h * .38f); quadraticTo(w * .62f, h * .52f, w * .5f, h * .62f); quadraticTo(w * .38f, h * .52f, w * .5f, h * .38f) }
            drawPath(flame, ink)
        }
        BedFamily.OTHER -> Unit
    }
}

/** The nozzle: brass filled, hardened steel striped; high flow longer; the tip grows from 0.2 to 1.2 mm. */
@Composable
fun NozzleDrawing(diameter: Double, hardened: Boolean, highFlow: Boolean, size: Dp = 32.dp) =
    Plate(size, "drawing_nozzle" + (if (hardened) "_hardened" else "") + (if (highFlow) "_hf" else "")) { ink ->
        val w = this.size.width; val h = this.size.height
        val bodyBottom = if (highFlow) h * .62f else h * .5f
        val tip = (w * (.06f + (diameter.coerceIn(0.2, 1.2) - 0.2).toFloat() * .14f))
        val body = Path().apply {
            moveTo(w * .32f, h * .15f); lineTo(w * .68f, h * .15f); lineTo(w * .68f, bodyBottom)
            lineTo(w * .5f + tip, h * .85f); lineTo(w * .5f - tip, h * .85f); lineTo(w * .32f, bodyBottom); close()
        }
        if (hardened) {
            drawPath(body, ink, style = Stroke(w * .05f))
            for (i in 1..3) drawLine(ink, Offset(w * .34f, h * (.15f + i * .1f)), Offset(w * .66f, h * (.08f + i * .1f)), w * .04f)
        } else drawPath(body, ink)
    }
