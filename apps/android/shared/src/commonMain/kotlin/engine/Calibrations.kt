package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull

/**
 * The engine's calibration tests on a phone (calibrations.md; `tisma-engine calibration`), as Android runs them: the
 * kinds in a useful order, the command's arguments, what it answers (the unit and the heights where each value is
 * printed) and, once the person says which came out best, the settings that change. Own design.
 */
object Calibrations {
    /** The FDM tests the engine makes, in a useful order (the same list as Android's). */
    val KINDS = listOf("first_layer", "temperature", "flow", "retraction", "retraction_speed", "pressure_advance",
        "max_flow", "speed", "acceleration", "bridges_overhangs", "dimensions", "elephant_foot", "fit")

    /** Kinds read by eye, with no value to apply (the engine's `calibration apply` refuses them). */
    val WITHOUT_VALUE = setOf("dimensions", "first_layer", "fit", "elephant_foot")

    /** One band of the test: from [z0] to [z1] mm the part is printed with [value]. */
    data class Segment(val z0: Double, val z1: Double, val value: Double)

    data class Test(val unit: String, val segments: List<Segment>)

    /** `calibration <kind> --config --model --settings` and, when given, the range (start, end, step). */
    fun args(kind: String, config: String, model: String, settings: String, range: List<Double>? = null): List<String> =
        listOf("calibration", kind, "--config", config, "--model", model, "--settings", settings) +
            (range?.takeIf { it.size == 3 }?.let { listOf("--start", num(it[0]), "--end", num(it[1]), "--step", num(it[2])) } ?: emptyList())

    /** `calibration apply <kind> --value --config -o`: the settings with the chosen value written in. */
    fun applyArgs(kind: String, value: Double, config: String, out: String): List<String> =
        listOf("calibration", "apply", kind, "--value", num(value), "--config", config, "-o", out)

    /** What `calibration` prints: its unit and segments; null when it is not that answer. */
    fun parse(stdout: String): Test? {
        val o = runCatching { Json.parseToJsonElement(stdout) as? JsonObject }.getOrNull() ?: return null
        val segs = (o["segments"] as? JsonArray).orEmpty().mapNotNull { s ->
            when (s) {
                is JsonObject -> {
                    fun d(k: String) = (s[k] as? JsonPrimitive)?.doubleOrNull
                    Segment(d("z0") ?: return@mapNotNull null, d("z1") ?: return@mapNotNull null, d("value") ?: return@mapNotNull null)
                }
                // docs/PROJECT.md writes them as [z0, z1, value].
                is JsonArray -> {
                    val v = s.mapNotNull { (it as? JsonPrimitive)?.doubleOrNull }
                    if (v.size == 3) Segment(v[0], v[1], v[2]) else null
                }
                else -> null
            }
        }
        return Test((o["unit"] as? JsonPrimitive)?.contentOrNull.orEmpty(), segs)
    }

    /**
     * The settings `calibration apply` changed: the keys (`a.b`) whose value in [applied] differs from [before], with
     * their new value, to keep as the person's changes.
     */
    fun changed(before: String, applied: String): Map<String, JsonElement> {
        fun flat(t: String) = runCatching { (Json.parseToJsonElement(t) as? JsonObject)?.let { SettingsLayout.flatten(it) } }
            .getOrNull().orEmpty()
        val a = flat(before)
        // Leaves only: the flattening also lists each nested object under its own key.
        return flat(applied).filter { (k, v) -> v !is JsonObject && a[k] != v }
    }

    private fun num(d: Double): String = if (d == kotlin.math.floor(d) && kotlin.math.abs(d) < 1e15) d.toLong().toString() else d.toString()
}
