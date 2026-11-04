package net.tisma.app.engine

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put

/**
 * Engineering on a bare model (mobile_app.md, rule 13): the faces marked as
 * fixed or loaded by touch, the material, the quality and the safety the
 * part must keep, written as the engine's case (docs/ANALYSIS.md, «El caso»).
 * Each face goes as a box around it, in the model file's coordinates.
 */
data class EngineeringCase(
    val conditions: List<Condition> = emptyList(),
    val material: String = "bambu-pla-basic",
    val quality: String = "normal",
    val requiredSafety: Double = 2.0,
) {
    enum class Kind { FIXED, LOAD }

    data class Condition(
        val kind: Kind,
        /** The marked triangles of the shown mesh, to draw them. */
        val triangles: IntArray,
        /** The box around them in the shown (bed) coordinates: min x y z, max x y z. */
        val box: FloatArray,
        /** The total force in N, for a load. */
        val newtons: Double = 100.0,
        /** Its direction, unit length; by default into the face. */
        val direction: FloatArray = floatArrayOf(0f, 0f, -1f),
        /**
         * The engine's own face (`bottom`, `top`, `x_min`, …) when the touched
         * face is the part's end in that axis: exact on the voxels, where a box
         * depends on how the grid falls (measured on a beam: the same safety
         * as the engine's faces with a 0.08 mm pad at normal quality, 11 % off
         * with 0.3 mm, nothing held under 0.08 mm).
         */
        val face: String? = null,
    )

    /** The case JSON; [offset] is how far the shown mesh was moved from the file's coordinates. */
    fun toJson(offset: FloatArray): JsonObject = buildJsonObject {
        put("material", material)
        put("quality", quality)
        fun region(c: Condition) = c.face?.let { JsonPrimitive(it) } ?: buildJsonObject {
            put("min", buildJsonArray { for (a in 0..2) add(JsonPrimitive((c.box[a] - offset[a]).toDouble())) })
            put("max", buildJsonArray { for (a in 0..2) add(JsonPrimitive((c.box[a + 3] - offset[a]).toDouble())) })
        }
        put("fixtures", JsonArray(conditions.filter { it.kind == Kind.FIXED }.mapIndexed { i, c ->
            buildJsonObject { put("name", "fixed ${i + 1}"); put(if (c.face != null) "face" else "box", region(c)) }
        }))
        put("loads", JsonArray(conditions.filter { it.kind == Kind.LOAD }.mapIndexed { i, c ->
            buildJsonObject {
                put("name", "load ${i + 1}")
                put(if (c.face != null) "face" else "box", region(c))
                put("force", buildJsonArray { for (a in 0..2) add(JsonPrimitive(c.newtons * c.direction[a])) })
            }
        }))
    }

    companion object {
        /**
         * A box pad that catches the face's voxels: under half a voxel, which
         * is half a line width at normal quality and a quarter at high
         * (docs/ANALYSIS.md, `quality`), for 0.4 mm lines.
         */
        fun pad(quality: String) = if (quality == "high") 0.04f else 0.08f

        /** The engine's face name when [triangles] (normal [n]) lie on the part's end in an axis, or null. */
        fun endFace(mesh: MeshFile, triangles: IntArray, n: FloatArray): String? {
            val axis = (0..2).firstOrNull { kotlin.math.abs(n[it]) > 0.996f } ?: return null
            val b = mesh.bounds
            val max = n[axis] > 0
            val target = if (max) b[axis + 3] else b[axis]
            for (t in triangles) for (k in 0..2) if (kotlin.math.abs(mesh.positions[9 * t + 3 * k + axis] - target) > 0.01f) return null
            return when (axis) {
                0 -> if (max) "x_max" else "x_min"
                1 -> if (max) "y_max" else "y_min"
                else -> if (max) "top" else "bottom"
            }
        }

        /** The materials of docs/MATERIALS.md the phone offers (their data sheets' records). */
        val MATERIALS = listOf("bambu-pla-basic", "bambu-pla-matte", "bambu-pla-tough-plus", "bambu-petg-basic",
            "bambu-petg-hf", "bambu-abs", "bambu-asa", "bambu-pla-cf", "bambu-petg-cf", "bambu-pa6-cf",
            "bambu-paht-cf", "esun-pla-plus", "esun-petg-basic")

    }
}
