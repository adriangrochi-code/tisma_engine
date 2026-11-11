package net.tisma.cloud

import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.longOrNull

// What the Tisma Cloud API v1 sends back (docs/spec/features/cloud.md). Every
// reader is lenient: a missing or mistyped field gets its empty value, so a
// server that adds fields or leaves one out never breaks the app.

/** The account, `GET /v1/me` (rule 5a: the hours are shown as they come). */
data class Account(
    val email: String,
    val plan: String,
    val paid: Boolean,
    val paidUntil: String,
    val hoursIncluded: Double,
    val hoursUsed: Double,
    val hoursLeft: Double,
    val activeJobs: Int,
    val maxConcurrent: Int,
    /** No hour limit. */
    val unlimited: Boolean = false,
    /** How many printers the plan lets the account use from outside (cloud.md, rule 16). */
    val remotePrinters: Int = 0,
    /** False for an account made with Google only. */
    val hasPassword: Boolean = true,
    /** cloud.md 5c: the store subscription that gives the plan, or null. */
    val subscription: Subscription? = null,
    /** cloud.md 5c (df50c4e): given to the store when buying (Play's obfuscatedAccountId, StoreKit's appAccountToken). */
    val purchaseAccountToken: String = "",
) {
    companion object {
        fun from(o: JsonObject) = Account(
            email = o.text("email"),
            plan = o.text("plan"),
            paid = o.bool("paid"),
            paidUntil = o.text("paid_until"),
            hoursIncluded = o.number("hours_included"),
            hoursUsed = o.number("hours_used"),
            hoursLeft = o.number("hours_left"),
            activeJobs = o.number("active_jobs").toInt(),
            maxConcurrent = o.number("max_concurrent").toInt(),
            unlimited = o.bool("unlimited"),
            remotePrinters = o.number("remote_printers").toInt(),
            hasPassword = (o["has_password"] as? JsonPrimitive)?.booleanOrNull ?: true,
            subscription = (o["subscription"] as? JsonObject)?.let(Subscription::from),
            purchaseAccountToken = o.text("purchase_account_token"),
        )
    }
}

/** cloud.md 5c: `subscription` of `GET /v1/me`. */
data class Subscription(val store: String, val productId: String, val expires: String, val autoRenew: Boolean) {
    companion object {
        fun from(o: JsonObject) = Subscription(o.text("store"), o.text("product_id"), o.text("expires"), o.bool("auto_renew"))
    }
}

/** cloud.md 5c: what a store gave as the proof of a purchase. */
data class StorePurchase(val store: String, val productId: String, val purchaseToken: String = "",
                         val signedTransaction: String = "", val storeIdKey: String = "")

/** An open session of the account, `GET /v1/me/sessions`. */
data class AccountSession(val id: String, val device: String, val createdAt: String, val lastUsed: String, val current: Boolean) {
    companion object {
        fun from(o: JsonObject) = AccountSession(o.text("id"), o.text("device"), o.text("created_at"), o.text("last_used"),
            o.bool("current"))
    }
}

/** A public plan, `GET /v1/plans` (rule 5a), with the job classes it includes (rule 10b). */
data class Plan(
    val name: String,
    val monthlyHours: Double,
    val maxConcurrent: Int,
    val priceCents: Long,
    val currency: String,
    val classes: List<String>,
    /** cloud.md 5c: the plan's product in each store where it is sold. */
    val storeProducts: Map<String, String> = emptyMap(),
) {
    fun includes(jobClass: String) = jobClass in classes

    companion object {
        fun from(o: JsonObject) = Plan(
            name = o.text("name"),
            monthlyHours = o.number("monthly_hours"),
            maxConcurrent = o.number("max_concurrent").toInt(),
            priceCents = o.number("price_cents").toLong(),
            currency = o.text("currency"),
            classes = o.texts("classes"),
            storeProducts = (o["store_products"] as? JsonObject)?.mapNotNull { (k, v) ->
                (v as? JsonPrimitive)?.contentOrNull?.takeIf { it.isNotBlank() }?.let { k to it }
            }?.toMap().orEmpty(),
        )
    }
}

enum class JobStatus(val wire: String) {
    QUEUED("queued"), RUNNING("running"), DONE("done"), FAILED("failed"), CANCELLED("cancelled"), UNKNOWN("");

    val finished get() = this == DONE || this == FAILED || this == CANCELLED

    companion object {
        fun of(text: String) = entries.firstOrNull { it.wire == text && it != UNKNOWN } ?: UNKNOWN
    }
}

/** A job, `GET /v1/jobs/{id}` (rule 7). */
data class Job(
    val id: String,
    val kind: String,
    val jobClass: String,
    val params: JsonObject,
    val status: JobStatus,
    val progress: Int,
    val seconds: Double,
    val error: String,
    val summary: JsonObject,
    val resultFilename: String,
    val createdAt: String,
    val startedAt: String,
    val finishedAt: String,
) {
    companion object {
        fun from(o: JsonObject) = Job(
            id = o.text("id"),
            kind = o.text("kind"),
            jobClass = o.text("job_class"),
            params = o["params"] as? JsonObject ?: JsonObject(emptyMap()),
            status = JobStatus.of(o.text("status")),
            progress = o.number("progress").toInt().coerceIn(0, 100),
            seconds = o.number("seconds"),
            error = o.text("error"),
            summary = o["summary"] as? JsonObject ?: JsonObject(emptyMap()),
            resultFilename = o.text("result_filename"),
            createdAt = o.text("created_at"),
            startedAt = o.text("started_at"),
            finishedAt = o.text("finished_at"),
        )
    }
}

/** One of the account's own problem reports, `GET /v1/me/reports` (rule 11). */
data class SentReport(
    val id: String,
    val kind: String,
    val title: String,
    val status: String,
    val categoryName: String,
    val createdAt: String,
    val updatedAt: String,
    val rewardedAt: String,
) {
    companion object {
        fun from(o: JsonObject) = SentReport(
            id = o.text("id"),
            kind = o.text("kind"),
            title = o.text("title"),
            status = o.text("status"),
            categoryName = o.text("category_name"),
            createdAt = o.text("created_at"),
            updatedAt = o.text("updated_at"),
            rewardedAt = o.text("rewarded_at"),
        )
    }
}

/**
 * A part scanned with Tisma Scanner and sent with «Imprimir» (p2p_library.md, rule 7 bis): private to the account,
 * waiting to reach «Mi biblioteca». [sha256] checks the mesh when the server gives it.
 */
data class Scan(val id: String, val name: String, val file: String, val size: Long, val sha256: String, val created: String) {
    companion object {
        fun from(o: JsonObject) = Scan(o.text("id"), o.text("name"), o.text("file"), o.number("size").toLong(),
            o.text("sha256").lowercase(), o.text("created"))
    }
}

/** A downloaded result: its name and where it was written. */
data class Download(val filename: String, val bytes: Long)

// Lenient readers. Ids may come as numbers; they are kept as their digits.

/** Lenient field readers, also used by the cloud client (another module since they moved here). */
fun JsonObject.text(key: String): String {
    val p = this[key] as? JsonPrimitive ?: return ""
    if (p is JsonNull) return ""
    return p.contentOrNull ?: ""
}

fun JsonObject.number(key: String): Double {
    val p = this[key] as? JsonPrimitive ?: return 0.0
    if (p.isString) return 0.0
    return p.doubleOrNull ?: p.longOrNull?.toDouble() ?: 0.0
}

fun JsonObject.bool(key: String): Boolean = (this[key] as? JsonPrimitive)?.booleanOrNull ?: false

fun JsonObject.texts(key: String): List<String> =
    (this[key] as? kotlinx.serialization.json.JsonArray)?.mapNotNull { (it as? JsonPrimitive)?.contentOrNull }
        ?: emptyList()

/** The summary value as text for display: strings as they are, numbers and the rest as JSON. */
fun JsonElement.display(): String = when (this) {
    is JsonPrimitive -> if (this is JsonNull) "" else content
    else -> toString()
}
