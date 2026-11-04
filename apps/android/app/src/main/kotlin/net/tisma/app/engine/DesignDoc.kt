package net.tisma.app.engine

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.add
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put

/**
 * A design of the Design tab on the phone (cad_design.md), written in the
 * engine's own format (`tisma-design-1`, read by `tisma-engine design`): the
 * features in order, each a primitive or an operation on a body made before
 * it. The engine checks it and builds it through SolidCraft. Own design over
 * the engine's format.
 */
data class DesignDoc(val name: String = "Design", val features: List<Feature> = emptyList()) {

    sealed interface Feature {
        val name: String
        val suppressed: Boolean
        fun json(): JsonObject
    }

    /** A primitive standing on z = 0, centred on the origin (sizes as the engine lists them, mm). */
    data class Primitive(override val name: String, val kind: String, val sizes: List<Double>,
                         override val suppressed: Boolean = false) : Feature {
        override fun json() = buildJsonObject {
            put("name", name); put("kind", kind)
            put("sizes", buildJsonArray { sizes.forEach { add(it) } })
            put("suppressed", suppressed)
        }
    }

    /** An operation on [target]: move, combine, mirror, the patterns, shell (the engine's names). */
    data class Operation(
        override val name: String,
        val kind: String,
        val target: String,
        val value: Double = 2.0,
        val offset: List<Double> = listOf(0.0, 0.0, 0.0),
        val angle: Double = 0.0,
        val axis: String = "Z",
        val copy: Boolean = false,
        val count: Int = 6,
        val direction: List<Double> = listOf(1.0, 0.0, 0.0),
        val spacing: Double = 30.0,
        val plane: String = "YZ",
        val tools: List<String> = emptyList(),
        val boolean: String = "cut",
        override val suppressed: Boolean = false,
    ) : Feature {
        override fun json() = buildJsonObject {
            put("name", name); put("kind", "operation"); put("suppressed", suppressed)
            put("operation", buildJsonObject {
                put("kind", kind); put("target", target); put("value", value)
                put("refs", JsonArray(emptyList()))
                put("offset", buildJsonArray { offset.forEach { add(it) } })
                put("angle", angle); put("axis", axis); put("copy", copy); put("count", count)
                put("direction", buildJsonArray { direction.forEach { add(it) } })
                put("spacing", spacing); put("plane", plane)
                put("tools", buildJsonArray { tools.forEach { add(it) } })
                put("boolean", boolean)
            })
        }
    }

    fun json(): JsonObject = buildJsonObject {
        put("format", "tisma-design-1")
        put("name", name)
        put("features", JsonArray(features.map { it.json() }))
    }

    /** A name not used yet: «Box 1», «Box 2»… */
    fun freshName(base: String): String {
        var i = 1
        while (features.any { it.name == "$base $i" }) i++
        return "$base $i"
    }

    /** The bodies an operation can work on: every feature before it that is not suppressed. */
    fun bodies(): List<String> = features.filter { !it.suppressed }.map { it.name }

    companion object {
        /** The primitives and their sizes' starting values (src/cad/design.hpp: default_sizes). */
        val PRIMITIVES = linkedMapOf(
            "box" to listOf(20.0, 20.0, 20.0),
            "cylinder" to listOf(20.0, 20.0),
            "sphere" to listOf(20.0),
            "cone" to listOf(20.0, 0.0, 20.0),
            "tube" to listOf(20.0, 14.0, 20.0),
            "torus" to listOf(20.0, 5.0),
        )

        /** The operations the phone offers without picking edges or faces. */
        val OPERATIONS = listOf("move", "combine", "mirror", "pattern_circular", "pattern_rectangular", "shell")

        fun primitive(doc: DesignDoc, kind: String, label: String) =
            Primitive(doc.freshName(label), kind, PRIMITIVES.getValue(kind))
    }
}
