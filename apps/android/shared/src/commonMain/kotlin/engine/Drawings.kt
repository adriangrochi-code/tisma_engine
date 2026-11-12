package net.tisma.app.engine

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlin.math.hypot

// Which drawing the printer card shows (settings_panel.md, rules 34 to 36): the same tables on Android and the iPhone.

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
fun roundBed(bed: JsonElement?): Boolean {
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

