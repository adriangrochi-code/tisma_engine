package net.tisma.app.engine

/** One line of an Engineering result: its key (verdict, min_safety, warning…) and value. */
data class ReportLine(val key: String, val value: String)

/**
 * Engineering on the iPhone (mobile_app.md, rule 13; docs/ANALYSIS.md): the case and the report as Android writes and
 * reads them. The iPhone has no 3D picking yet, so a face is one of the engine's own ([faces]).
 */
object EngineeringBridge {
    val faces: List<String> get() = EngineeringCase.FACES.keys.toList()
    val materials: List<String> get() = EngineeringCase.MATERIALS

    /** The case JSON: [fixedFace] held, [newtons] pushed into [loadFace]; «solid» is the quick first idea. */
    fun caseJson(fixedFace: String, loadFace: String, newtons: Double, material: String, quality: String, solid: Boolean): String {
        fun cond(kind: EngineeringCase.Kind, face: String, n: Double = 100.0) = EngineeringCase.Condition(kind, IntArray(0),
            FloatArray(6), newtons = n, direction = EngineeringCase.FACES[face] ?: floatArrayOf(0f, 0f, -1f), face = face)
        return EngineeringCase(listOf(cond(EngineeringCase.Kind.FIXED, fixedFace), cond(EngineeringCase.Kind.LOAD, loadFace, newtons)),
            material, quality, solid = solid).toJson(FloatArray(3)).toString()
    }

    /** `analyze <model> --config --case -o` (exit 0, or 5 when the report says why there is no result). */
    fun analyzeArgs(model: String, config: String, caseFile: String, out: String): List<String> =
        listOf("analyze", model, "--config", config, "--case", caseFile, "-o", out)

    fun report(json: String, requiredSafety: Double): List<ReportLine> =
        EngineeringCase.reportLines(json, requiredSafety).map { ReportLine(it.first, it.second) }
}
