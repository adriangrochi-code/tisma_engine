package net.tisma.app.ui

import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import net.tisma.app.R
import net.tisma.app.cloud.Notice
import net.tisma.cloud.CloudException
import net.tisma.cloud.Job
import net.tisma.cloud.JobStatus
import net.tisma.cloud.failure
import net.tisma.cloud.message
import java.util.Locale

// The words the screens use for what the cloud sends.

/** Whether the interface is in Spanish (cloud.md, rule 19a: then `error_es` is shown). */
fun Context.isSpanish(): Boolean = resources.configuration.locales[0].language == "es"

/** A server error, worded by its kind (cloud.md, rule 4): the server's text is always shown. */
fun Context.errorText(e: CloudException): String = when (e.kind) {
    CloudException.Kind.NETWORK -> getString(R.string.error_network)
    CloudException.Kind.UNEXPECTED -> getString(R.string.error_unexpected)
    CloudException.Kind.EXPIRED -> getString(R.string.error_expired)
    CloudException.Kind.TOO_LARGE -> getString(R.string.error_too_large)
    CloudException.Kind.BAD_PARAMS -> getString(R.string.error_bad_params, e.detail)
    CloudException.Kind.TOO_MANY -> getString(R.string.error_too_many)
    CloudException.Kind.PLAN -> getString(R.string.error_plan, e.detail)
    CloudException.Kind.SERVER -> getString(R.string.error_server, e.detail)
}

fun Context.noticeText(n: Notice): String = when (n) {
    is Notice.Failed -> errorText(n.error)
    Notice.SessionExpired -> getString(R.string.error_expired)
    Notice.NotThreeMf -> getString(R.string.error_not_3mf)
    Notice.CannotRead -> getString(R.string.error_cannot_read)
    Notice.CodeSent -> getString(R.string.notice_code_sent)
    Notice.PasswordChanged -> getString(R.string.notice_password_changed)
    is Notice.ReportSent -> getString(R.string.report_sent) +
        if (n.similar > 0) " " + getString(R.string.report_similar, n.similar) else ""
    is Notice.Saved -> getString(R.string.notice_saved, n.name)
}

/** The text for a failed job's `code` (rule 19a); null for the generic `error` and unknown codes. */
fun Context.jobCodeText(code: String): String? = when (code) {
    "bad_mode" -> getString(R.string.code_bad_mode)
    "bad_quality" -> getString(R.string.code_bad_quality)
    "not_in_cloud" -> getString(R.string.code_not_in_cloud)
    "needs_conditions" -> getString(R.string.code_needs_conditions)
    "no_valid_result" -> getString(R.string.code_no_valid_result)
    "usage" -> getString(R.string.code_usage)
    else -> null
}

/** What a failed job says to the person, or null. */
fun Context.jobFailureText(job: Job): String? =
    job.failure()?.message(isSpanish()) { jobCodeText(it) }?.ifBlank { null }

fun Context.statusText(s: JobStatus): String = getString(when (s) {
    JobStatus.QUEUED -> R.string.status_queued
    JobStatus.RUNNING -> R.string.status_running
    JobStatus.DONE -> R.string.status_done
    JobStatus.FAILED -> R.string.status_failed
    JobStatus.CANCELLED -> R.string.status_cancelled
    JobStatus.UNKNOWN -> R.string.status_unknown
})

fun Context.kindText(job: Job): String {
    val mode = (job.params["mode"] as? JsonPrimitive)?.contentOrNull
    return getString(when (job.kind) {
        "slice" -> R.string.kind_slice
        "fea" -> if (mode == "lightest-infill") R.string.kind_fea_optimize else R.string.kind_fea
        "aero" -> R.string.kind_aero
        "orient" -> R.string.kind_orient
        else -> return job.kind
    })
}

/** A plan class (rule 10b) in words. */
fun Context.classText(c: String): String = when (c) {
    "slice" -> getString(R.string.class_slice)
    "slice_large" -> getString(R.string.class_slice_large)
    "fea" -> getString(R.string.class_fea)
    "fea_optimize" -> getString(R.string.class_fea_optimize)
    "aero" -> getString(R.string.class_aero)
    "orient" -> getString(R.string.class_orient)
    else -> c
}

fun Context.reportStatusText(s: String): String = when (s) {
    "new" -> getString(R.string.report_status_new)
    "confirmed" -> getString(R.string.report_status_confirmed)
    "fixed" -> getString(R.string.report_status_fixed)
    "wontfix" -> getString(R.string.report_status_wontfix)
    "duplicate" -> getString(R.string.report_status_duplicate)
    else -> s
}

/** Hours as the server sends them, with at most two decimals (rule 5a: nothing is added). */
fun hours(h: Double): String {
    val s = String.format(Locale.getDefault(), "%.2f", h)
    return s.trimEnd('0').trimEnd('.', ',')
}

/** A price from `price_cents` and `currency`. */
fun price(cents: Long, currency: String): String {
    val amount = String.format(Locale.getDefault(), "%.2f", cents / 100.0)
    return if (currency.isBlank()) amount else "$amount ${currency.uppercase(Locale.ROOT)}"
}

/** The date part of an ISO time, as the server sends it. */
fun shortDate(iso: String): String = iso.replace('T', ' ').take(16)

@Composable
fun ctx(): Context = LocalContext.current

/**
 * A job's summary in words (QA AND4): known keys get a label and a unit, the
 * time in seconds is left out when the text time is there, and unknown keys
 * are shown as they come.
 */
fun Context.readableSummary(lines: List<Pair<String, String>>): List<Pair<String, String>> {
    val keys = lines.map { it.first }.toSet()
    fun num(v: String) = v.toDoubleOrNull()
    fun fmt(pattern: String, v: Double) = String.format(Locale.getDefault(), pattern, v)
    return lines.mapNotNull { (k, v) ->
        when (k) {
            "print_time" -> getString(R.string.plate_time) to v
            "print_time_s" -> if ("print_time" in keys) null
                else getString(R.string.plate_time) to (num(v)?.let { duration(it.toLong()) } ?: v)
            "filament_mm" -> getString(R.string.plate_filament) to (num(v)?.let { fmt("%.2f m", it / 1000) } ?: v)
            "filament_g" -> getString(R.string.summary_weight) to (num(v)?.let { fmt("%.1f g", it) } ?: v)
            "layers" -> getString(R.string.plate_layers) to v
            "verdict" -> getString(R.string.summary_verdict) to v
            "min_safety" -> getString(R.string.summary_min_safety) to (num(v)?.let { fmt("%.2f", it) } ?: v)
            "max_displacement_mm" -> getString(R.string.summary_displacement) to (num(v)?.let { fmt("%.3f mm", it) } ?: v)
            "mass_g" -> getString(R.string.summary_mass) to (num(v)?.let { fmt("%.1f g", it) } ?: v)
            "drag_n" -> getString(R.string.summary_drag) to (num(v)?.let { fmt("%.4f N", it) } ?: v)
            "cd" -> getString(R.string.summary_cd) to (num(v)?.let { fmt("%.3f", it) } ?: v)
            else -> k to v
        }
    }
}
