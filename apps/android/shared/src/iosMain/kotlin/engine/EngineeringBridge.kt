package net.tisma.app.engine

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import net.tisma.io.FileInput
import platform.Foundation.NSData
import platform.Foundation.create
import platform.posix.memcpy

/** One line of an Engineering result: its key (verdict, min_safety, warning…) and value. */
data class ReportLine(val key: String, val value: String)

/**
 * A face for the case: the triangles touched on the part (to draw them), the engine's own face name when it is the
 * part's end in an axis, the box around it otherwise (min x y z, max x y z, in the model file's coordinates), and its
 * outward normal (a load pushes into it).
 */
data class PickedFace(val triangles: List<Int>, val face: String?, val box: List<Double>, val normal: List<Double>)

/** The safety map as SceneKit takes it: float x y z per corner and float r g b per corner, three corners a triangle. */
data class StressMapData(val positions: NSData, val colors: NSData, val triangles: Int)

/**
 * Engineering on the iPhone (mobile_app.md, rule 13; docs/ANALYSIS.md): the faces touched on the part, the case and the
 * report, with the same code as Android ([FacePicking], [EngineeringCase], [StressMap]).
 */
@OptIn(ExperimentalForeignApi::class)
object EngineeringBridge {
    val faces: List<String> get() = EngineeringCase.FACES.keys.toList()
    val materials: List<String> get() = EngineeringCase.MATERIALS

    private var positions = FloatArray(0)
    private var normals = FloatArray(0)
    private var bounds = FloatArray(6)

    /** The shown part: float x y z per corner, three corners a triangle, in the model file's coordinates. */
    fun setMesh(soup: NSData): Int {
        val n = soup.length.toInt() / 4
        positions = FloatArray(n).also { a -> if (n > 0) a.usePinned { memcpy(it.addressOf(0), soup.bytes, soup.length) } }
        normals = FacePicking.normals(positions)
        bounds = FacePicking.bounds(positions)
        return positions.size / 9
    }

    /** The flat face around the touched [triangle], or null when there is no such triangle. */
    fun pick(triangle: Int, quality: String): PickedFace? {
        if (triangle < 0 || triangle >= positions.size / 9) return null
        val tris = FacePicking.flatFace(positions, normals, triangle)
        val n = FacePicking.normal(positions, tris)
        val face = FacePicking.endFace(positions, bounds, tris, n)
        return PickedFace(tris.toList(), face, FacePicking.box(positions, tris, EngineeringCase.pad(quality)).map { it.toDouble() },
            n.map { it.toDouble() })
    }

    /**
     * A load at a point (mobile_app.md, rule 13: «en un punto o en toda la cara»): the touched [triangle] at the
     * point ([x], [y], [z], the model file's coordinates), given to the engine as a box of [radius] mm around it.
     */
    fun point(triangle: Int, x: Double, y: Double, z: Double, radius: Double): PickedFace? {
        if (triangle < 0 || triangle >= positions.size / 9) return null
        val n = FacePicking.normal(positions, intArrayOf(triangle))
        return PickedFace(listOf(triangle), null, listOf(x - radius, y - radius, z - radius, x + radius, y + radius, z + radius),
            n.map { it.toDouble() })
    }

    /** The directions a load can push: into its face, or along an axis («dirección», mobile_app.md, rule 13). */
    val directions: List<String> get() = listOf("into", "-z", "+z", "-x", "+x", "-y", "+y")

    /** The case with the load pushed along [direction] (one of [directions]) instead of into its face. */
    fun caseAlong(fixed: PickedFace, loaded: PickedFace, newtons: Double, direction: String, material: String, quality: String,
                  solid: Boolean): String {
        val along = when (direction) {
            "-z" -> listOf(0.0, 0.0, 1.0); "+z" -> listOf(0.0, 0.0, -1.0)
            "-x" -> listOf(1.0, 0.0, 0.0); "+x" -> listOf(-1.0, 0.0, 0.0)
            "-y" -> listOf(0.0, 1.0, 0.0); "+y" -> listOf(0.0, -1.0, 0.0)
            else -> null
        }
        // caseFor pushes against the normal: an axis is given as the normal it pushes against.
        return caseFor(fixed, if (along == null) loaded else loaded.copy(normal = along), newtons, material, quality, solid)
    }

    /**
     * The settings with the nozzle's temperature (°C) the analysis assumes: it enters the adhesion between layers and
     * lines (docs/ANALYSIS.md; mobile_app.md, rule 13, «la temperatura»). 0 or less keeps the filament's.
     */
    fun withNozzleTemperature(configJson: String, celsius: Double): String =
        if (celsius <= 0) configJson else run {
            val base = runCatching { kotlinx.serialization.json.Json.parseToJsonElement(configJson) as? kotlinx.serialization.json.JsonObject }
                .getOrNull() ?: kotlinx.serialization.json.JsonObject(emptyMap())
            val vars = kotlinx.serialization.json.JsonObject(mapOf("machine" to kotlinx.serialization.json.JsonObject(mapOf("variables" to
                kotlinx.serialization.json.JsonObject(mapOf("temperature" to kotlinx.serialization.json.JsonPrimitive(celsius),
                    "first_layer_temperature" to kotlinx.serialization.json.JsonPrimitive(celsius)))))))
            PrintChanges.merge(base, vars).toString()
        }

    /** One of the engine's own faces, chosen by name. */
    fun named(face: String): PickedFace {
        val into = EngineeringCase.FACES[face] ?: floatArrayOf(0f, 0f, -1f)
        return PickedFace(emptyList(), face, emptyList(), into.map { -it.toDouble() })
    }

    /** The case JSON: [fixed] held, [newtons] pushed into [loaded]; «solid» is the quick first idea. */
    fun caseFor(fixed: PickedFace, loaded: PickedFace, newtons: Double, material: String, quality: String, solid: Boolean): String {
        fun cond(kind: EngineeringCase.Kind, f: PickedFace, n: Double = 100.0) = EngineeringCase.Condition(kind,
            f.triangles.toIntArray(), if (f.box.size == 6) FloatArray(6) { f.box[it].toFloat() } else FloatArray(6), newtons = n,
            direction = FloatArray(3) { -f.normal[it].toFloat() }, face = f.face ?: if (f.box.size == 6) null else "bottom")
        return EngineeringCase(listOf(cond(EngineeringCase.Kind.FIXED, fixed), cond(EngineeringCase.Kind.LOAD, loaded, newtons)),
            material, quality, solid = solid).toJson(FloatArray(3)).toString()
    }

    /** The case with the engine's faces by name. */
    fun caseJson(fixedFace: String, loadFace: String, newtons: Double, material: String, quality: String, solid: Boolean): String =
        caseFor(named(fixedFace), named(loadFace), newtons, material, quality, solid)

    /**
     * `analyze <model> --config --case -o` (exit 0, or 5 when the report says why there is no result); with [mesh], the
     * voxel surface colored by safety too (`--mesh`).
     */
    fun analyzeArgs(model: String, config: String, caseFile: String, out: String, mesh: String): List<String> =
        listOf("analyze", model, "--config", config, "--case", caseFile, "-o", out) + (if (mesh.isEmpty()) emptyList() else listOf("--mesh", mesh))

    fun report(json: String, requiredSafety: Double): List<ReportLine> =
        EngineeringCase.reportLines(json, requiredSafety).map { ReportLine(it.first, it.second) }

    /** The safety map the engine wrote at [path] (at most [maxFaces] quads for the phone), or null. */
    fun stressMap(path: String, maxFaces: Int): StressMapData? = runCatching {
        val m = FileInput(path).use { StressMap.parse(it, maxFaces = maxFaces) }
        StressMapData(m.positions.toNSData(), m.colors.toNSData(), m.triangles)
    }.getOrNull()

    private fun FloatArray.toNSData(): NSData = if (isEmpty()) NSData() else usePinned { p ->
        NSData.create(bytes = p.addressOf(0), length = (size * 4).toULong())
    }
}
