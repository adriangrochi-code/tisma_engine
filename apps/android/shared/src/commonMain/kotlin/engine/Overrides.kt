package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive

/**
 * What the person changes in Print and Material on the phone (mobile_app.md, rule 7), as docs/CONFIG.md keys laid
 * over the printer's profile: layer height, infill density, supports and, when given, the nozzle and bed
 * temperatures (`machine.variables`, as Android's Engineering field). Common code for iOS (Android keeps its own).
 */
data class PrintChanges(
    val layerHeight: Double = 0.2,
    val infillPercent: Double = 20.0,
    val supports: Boolean = false,
    /** °C, or null to keep the filament's. */
    val nozzleTemp: Double? = null,
    val bedTemp: Double? = null,
) {
    fun toJson(): JsonObject = JsonObject(buildMap {
        put("layers", JsonObject(mapOf("height" to JsonPrimitive(layerHeight))))
        put("infill", JsonObject(mapOf("density" to JsonPrimitive(infillPercent))))
        put("support", JsonObject(mapOf("enabled" to JsonPrimitive(supports))))
        val vars = buildMap<String, JsonElement> {
            nozzleTemp?.let { put("temperature", JsonPrimitive(it)) }
            bedTemp?.let { put("bed_temperature", JsonPrimitive(it)) }
        }
        if (vars.isNotEmpty()) put("machine", JsonObject(mapOf("variables" to JsonObject(vars))))
    })

    /** The profile's JSON with these changes laid over it (objects merged key by key; the rest kept). */
    fun over(profileJson: String): String {
        val base = runCatching { Json.parseToJsonElement(profileJson) as? JsonObject }.getOrNull() ?: JsonObject(emptyMap())
        return merge(base, toJson()).toString()
    }

    companion object {
        fun merge(a: JsonObject, b: JsonObject): JsonObject = JsonObject(a.toMutableMap().also { m ->
            for ((k, v) in b) {
                val old = m[k]
                m[k] = if (old is JsonObject && v is JsonObject) merge(old, v) else v
            }
        })
    }
}
