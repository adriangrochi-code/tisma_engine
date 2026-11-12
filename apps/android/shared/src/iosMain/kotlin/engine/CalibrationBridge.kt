package net.tisma.app.engine

/** One band of a calibration test, for the SwiftUI list. */
data class CalSegment(val z0: Double, val z1: Double, val value: Double)

/** A calibration test the engine made: its unit and its bands (none for a test read by eye). */
data class CalTest(val unit: String, val segments: List<CalSegment>)

/** The calibration tests for the iPhone (calibrations.md; parity P5), with the shared [Calibrations]. */
object CalibrationBridge {
    val kinds: List<String> get() = Calibrations.KINDS

    fun byEye(kind: String): Boolean = kind in Calibrations.WITHOUT_VALUE

    /** The arguments; [start], [end] and [step] all 0 for the range the settings suggest. */
    fun args(kind: String, config: String, model: String, settings: String, start: Double, end: Double, step: Double): List<String> =
        Calibrations.args(kind, config, model, settings, if (step > 0 && start != end) listOf(start, end, step) else null)

    fun applyArgs(kind: String, value: Double, config: String, out: String): List<String> = Calibrations.applyArgs(kind, value, config, out)

    fun parse(stdout: String): CalTest? =
        Calibrations.parse(stdout)?.let { t -> CalTest(t.unit, t.segments.map { CalSegment(it.z0, it.z1, it.value) }) }
}
