package net.tisma.app

import androidx.compose.ui.graphics.Color
import net.tisma.app.ui.visionTypeColor
import org.junit.Test
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.test.assertTrue

/**
 * AC2 (accessibility_touch.md, rule 3): the preview's line-type palette
 * through the simulation of Machado, Oliveira & Fernandes 2009 (severity 1,
 * on linear RGB) for the three types of color blindness; neighbors in the
 * legend keep a CIEDE2000 difference (Sharma, Wu & Dalal 2005) of at least 10.
 */
class ColorVisionTest {
    private val machado = mapOf(
        "protanopia" to doubleArrayOf(0.152286, 1.052583, -0.204868, 0.114503, 0.786281, 0.099216, -0.003882, -0.048116, 1.051998),
        "deuteranopia" to doubleArrayOf(0.367322, 0.860646, -0.227968, 0.280085, 0.672501, 0.047413, -0.011820, 0.042940, 0.968881),
        "tritanopia" to doubleArrayOf(1.255528, -0.076749, -0.178779, -0.078411, 0.930809, 0.147602, 0.004733, 0.691367, 0.303900),
    )

    private fun lin(c: Double) = if (c <= 0.04045) c / 12.92 else ((c + 0.055) / 1.055).pow(2.4)

    private fun simulate(c: Color, m: DoubleArray): DoubleArray {
        val r = lin(c.red.toDouble()); val g = lin(c.green.toDouble()); val b = lin(c.blue.toDouble())
        return doubleArrayOf(m[0] * r + m[1] * g + m[2] * b, m[3] * r + m[4] * g + m[5] * b, m[6] * r + m[7] * g + m[8] * b)
            .map { it.coerceIn(0.0, 1.0) }.toDoubleArray()
    }

    /** Linear sRGB to CIELAB (D65). */
    private fun lab(rgb: DoubleArray): DoubleArray {
        val (r, g, b) = Triple(rgb[0], rgb[1], rgb[2])
        val x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
        val y = 0.2126 * r + 0.7152 * g + 0.0722 * b
        val z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
        fun f(t: Double) = if (t > 216.0 / 24389) Math.cbrt(t) else (24389.0 / 27 * t + 16) / 116
        return doubleArrayOf(116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z)))
    }

    private fun deg(r: Double) = r * 180 / PI
    private fun rad(d: Double) = d * PI / 180

    private fun ciede2000(a: DoubleArray, b: DoubleArray): Double {
        val (l1, a1, b1) = Triple(a[0], a[1], a[2]); val (l2, a2, b2) = Triple(b[0], b[1], b[2])
        val c1 = sqrt(a1 * a1 + b1 * b1); val c2 = sqrt(a2 * a2 + b2 * b2)
        val cm = (c1 + c2) / 2
        val g = 0.5 * (1 - sqrt(cm.pow(7) / (cm.pow(7) + 25.0.pow(7))))
        val ap1 = (1 + g) * a1; val ap2 = (1 + g) * a2
        val cp1 = sqrt(ap1 * ap1 + b1 * b1); val cp2 = sqrt(ap2 * ap2 + b2 * b2)
        fun hue(bb: Double, ap: Double) = if (bb == 0.0 && ap == 0.0) 0.0 else (deg(atan2(bb, ap)) + 360) % 360
        val hp1 = hue(b1, ap1); val hp2 = hue(b2, ap2)
        val dl = l2 - l1; val dc = cp2 - cp1
        var dh = if (cp1 * cp2 == 0.0) 0.0 else hp2 - hp1
        if (dh > 180) dh -= 360 else if (dh < -180) dh += 360
        val dH = 2 * sqrt(cp1 * cp2) * sin(rad(dh / 2))
        val lm = (l1 + l2) / 2; val cpm = (cp1 + cp2) / 2
        val hm = when {
            cp1 * cp2 == 0.0 -> hp1 + hp2
            abs(hp1 - hp2) <= 180 -> (hp1 + hp2) / 2
            hp1 + hp2 < 360 -> (hp1 + hp2 + 360) / 2
            else -> (hp1 + hp2 - 360) / 2
        }
        val t = 1 - 0.17 * cos(rad(hm - 30)) + 0.24 * cos(rad(2 * hm)) + 0.32 * cos(rad(3 * hm + 6)) - 0.20 * cos(rad(4 * hm - 63))
        val sl = 1 + 0.015 * (lm - 50).pow(2) / sqrt(20 + (lm - 50).pow(2))
        val sc = 1 + 0.045 * cpm
        val sh = 1 + 0.015 * cpm * t
        val rt = -2 * sqrt(cpm.pow(7) / (cpm.pow(7) + 25.0.pow(7))) * sin(rad(60 * exp(-((hm - 275) / 25).pow(2))))
        return sqrt((dl / sl).pow(2) + (dc / sc).pow(2) + (dH / sh).pow(2) + rt * (dc / sc) * (dH / sh))
    }

    @Test
    fun `CIEDE2000 matches Sharma's test pair`() {
        // Sharma, Wu & Dalal 2005, table 1, pair 1: 2.0425.
        val d = ciede2000(doubleArrayOf(50.0, 2.6772, -79.7751), doubleArrayOf(50.0, 0.0, -82.7485))
        assertTrue(abs(d - 2.0425) < 1e-3, "$d")
    }

    @Test
    fun `AC2 neighbors in the legend stay apart for each type of color blindness`() {
        val types = listOf("External perimeter", "Perimeter", "Internal infill", "Solid infill", "Top solid infill",
            "Support material", "Skirt/Brim")
        val report = StringBuilder()
        var worst = Double.MAX_VALUE
        for ((name, m) in machado) {
            val colors = types.mapIndexed { i, t -> visionTypeColor(t, i, name) }
            for (i in 0 until colors.size - 1) {
                val d = ciede2000(lab(simulate(colors[i], m)), lab(simulate(colors[i + 1], m)))
                report.append("$name ${types[i]} / ${types[i + 1]}: ${"%.1f".format(d)}\n")
                worst = minOf(worst, d)
            }
        }
        println(report)
        assertTrue(worst >= 10, "smallest ΔE2000 $worst\n$report")
    }
}
