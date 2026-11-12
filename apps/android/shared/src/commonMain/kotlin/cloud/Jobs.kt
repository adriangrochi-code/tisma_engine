package net.tisma.cloud

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.put
import net.tisma.io.ByteInput

/** What a job asks for: its `kind` and its `params` (cloud.md, rule 6). */
sealed class JobRequest {
    abstract val kind: JobKind
    abstract fun params(): JsonObject

    /** The plan class this calculation needs (rule 10b). A slice may also run as `slice_large`. */
    abstract val jobClass: String

    data object Slice : JobRequest() {
        override val kind = JobKind.SLICE
        override val jobClass = "slice"
        override fun params() = JsonObject(emptyMap())
    }

    data class Fea(
        val mode: FeaMode,
        val quality: FeaQuality,
        val objectIndex: Int,
        val instance: Int,
        val solid: Boolean = false,
    ) : JobRequest() {
        override val kind = JobKind.FEA
        override val jobClass get() = if (mode == FeaMode.LIGHTEST_INFILL) "fea_optimize" else "fea"
        override fun params() = buildJsonObject {
            put("mode", mode.wire)
            put("quality", quality.wire)
            put("object", objectIndex)
            put("instance", instance)
            if (solid) put("solid", true)
        }
    }

    data class Aero(
        val objectIndex: Int,
        val instance: Int,
        val quality: AeroQuality,
        /** m/s, more than 0 and up to 100. */
        val speed: Double,
        val direction: AeroDirection,
    ) : JobRequest() {
        init {
            require(speed > 0 && speed <= 100) { "speed must be more than 0 and up to 100 m/s" }
        }

        override val kind = JobKind.AERO
        override val jobClass = "aero"
        override fun params() = buildJsonObject {
            put("object", objectIndex)
            put("instance", instance)
            put("quality", quality.wire)
            put("speed", speed)
            put("direction", direction.wire)
        }
    }

    data class Orient(val objectIndex: Int, val instance: Int) : JobRequest() {
        override val kind = JobKind.ORIENT
        override val jobClass = "orient"
        override fun params() = buildJsonObject {
            put("object", objectIndex)
            put("instance", instance)
        }
    }
}

enum class FeaMode(val wire: String) { ANALYZE("analyze"), LIGHTEST_INFILL("lightest-infill"), RESIN_STRUCTURE("resin-structure") }

enum class FeaQuality(val wire: String) { FAST("fast"), NORMAL("normal"), HIGH("high"), ULTRA("ultra") }

enum class AeroQuality(val wire: String) { FRICTION("friction"), FAST("fast"), NORMAL("normal"), HIGH("high") }

enum class AeroDirection(val wire: String) { X("x"), MINUS_X("-x"), Y("y"), MINUS_Y("-y"), Z("z"), MINUS_Z("-z") }

/** Sends a [JobRequest]. */
fun CloudClient.submit(request: JobRequest, filename: String, length: Long, open: () -> ByteInput): Job =
    submit(request.kind, request.params(), filename, length, open)

/** Whether a plan lets a calculation run (rule 10b). A slice counts if the plan has either slice class. */
fun Plan.allows(request: JobRequest): Boolean =
    if (request is JobRequest.Slice) includes("slice") || includes("slice_large") else includes(request.jobClass)

/** The plans that include a calculation, for «Included in the plan …». */
fun List<Plan>.including(request: JobRequest): List<Plan> = filter { it.allows(request) }

/**
 * Whether a file is a 3MF package: a ZIP with a `.model` part under `3D/` (formats.md
 * rule 11). The server answers anything else with 422; checking first saves
 * the upload.
 */
/**
 * Whether a file may be a 3MF package: it starts like a ZIP. The app uses this
 * light check before uploading and leaves the rest to the server (a 422 with
 * its own text), so a valid package a ZIP reader dislikes (for example, one
 * with data descriptors on stored entries) is never refused on the phone.
 */
fun looksLikeZip(input: ByteInput): Boolean {
    val head = ByteArray(4)
    var n = 0
    while (n < 4) {
        val r = input.read(head, n, 4 - n)
        if (r < 0) break
        n += r
    }
    return n == 4 && head[0] == 0x50.toByte() && head[1] == 0x4B.toByte() && head[2] == 3.toByte() && head[3] == 4.toByte()
}
/** Error codes of a failed job (rule 19a). */
object JobErrorCodes {
    val KNOWN = listOf("bad_mode", "bad_quality", "not_in_cloud", "needs_conditions", "no_valid_result", "usage", "error")
}

/** Where a failed job's message for the person comes from (rule 19a). */
data class JobFailure(val code: String, val error: String, val errorEs: String)

fun Job.failure(): JobFailure? {
    if (status != JobStatus.FAILED) return null
    fun s(key: String) = (summary[key] as? JsonPrimitive)?.contentOrNull.orEmpty()
    return JobFailure(code = s("code"), error = s("error").ifEmpty { error }, errorEs = s("error_es"))
}

/**
 * The message for the person (rule 19a): in Spanish, `error_es` when there is
 * one; else the text for `code` from [codeText] in the interface's language
 * (null for a code it does not know, and for the generic `error`); else the
 * English `error`.
 */
fun JobFailure.message(spanish: Boolean, codeText: (String) -> String?): String {
    if (spanish && errorEs.isNotBlank()) return errorEs
    if (code.isNotBlank()) codeText(code)?.let { return it }
    return error
}

/** Summary keys in display order, with nested objects flattened as `a.b` (the server may nest them). */
fun Job.summaryLines(): List<Pair<String, String>> {
    val out = mutableListOf<Pair<String, String>>()
    fun walk(o: JsonObject, prefix: String) {
        for ((k, v) in o) {
            if (v is JsonObject) walk(v, "$prefix$k.") else if (v !is kotlinx.serialization.json.JsonNull) out += "$prefix$k" to v.display()
        }
    }
    walk(summary, "")
    return out.filter { (k, _) -> k !in setOf("code", "error", "error_es") }
}
