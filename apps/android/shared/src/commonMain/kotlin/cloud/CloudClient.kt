package net.tisma.cloud

import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import net.tisma.io.ByteInput
import net.tisma.io.ByteOutput
import net.tisma.io.BytesOutput
import net.tisma.io.IOException
import kotlin.concurrent.Volatile

/**
 * The Tisma Cloud API v1 (docs/spec/features/cloud.md, rules 1 to 11).
 *
 * Every call blocks; the app runs them off the main thread. A failure is a
 * [CloudException]. The token is held here only while the app runs: keeping it
 * between runs is the job of the app's secret store (rule 3).
 */
class CloudClient(
    serverUrl: String,
    @Volatile var token: String? = null,
    private val http: Http = PlatformHttp,
) {
    val serverUrl: String = normalizeServer(serverUrl)

    // ---- Account (rules 2 to 5a) ----

    /** `GET /v1/health`: whether the server answers, and the address of the account's billing page. */
    fun health(): Health {
        val o = getObject("/v1/health", auth = false)
        return Health(ok = o.bool("ok"), billingUrl = o.text("billing_url"))
    }

    /** `POST /v1/auth/login`. On success the client keeps the token. */
    /** cloud.md 4a: `GET /v1/status`, without a session. */
    fun status(): ServiceStatus = ServiceStatus.from(getObject("/v1/status", auth = false))

    fun login(email: String, password: String, device: String): Session {
        val o = postObject("/v1/auth/login", buildJsonObject {
            put("email", email); put("password", password); put("device", device)
        }, auth = false)
        return sessionOf(o, email)
    }

    /** `POST /v1/auth/register`; the server mails a 6-digit code to confirm it ([verify]). */
    fun register(email: String, password: String, plan: String? = null) {
        postObject("/v1/auth/register", buildJsonObject {
            put("email", email); put("password", password); plan?.let { put("plan", it) }
        }, auth = false)
    }

    /** `POST /v1/auth/verify`: confirms the e-mail with its code and opens a session. */
    fun verify(email: String, code: String, device: String): Session {
        val o = postObject("/v1/auth/verify", buildJsonObject {
            put("email", email); put("code", code); put("device", device)
        }, auth = false)
        return sessionOf(o, email)
    }

    /** `POST /v1/auth/resend`. */
    fun resendCode(email: String) {
        postObject("/v1/auth/resend", buildJsonObject { put("email", email) }, auth = false)
    }

    /** `POST /v1/auth/password/forgot`. */
    fun forgotPassword(email: String) {
        postObject("/v1/auth/password/forgot", buildJsonObject { put("email", email) }, auth = false)
    }

    /** `POST /v1/auth/password/reset`. */
    fun resetPassword(email: String, code: String, newPassword: String) {
        postObject("/v1/auth/password/reset", buildJsonObject {
            put("email", email); put("code", code); put("password", newPassword)
        }, auth = false)
    }

    /** `POST /v1/auth/logout`. The client forgets the token even when the server cannot be reached. */
    fun logout() {
        try {
            postObject("/v1/auth/logout", JsonObject(emptyMap()))
        } finally {
            token = null
        }
    }

    /** `GET /v1/me`. */
    fun account(): Account = Account.from(getObject("/v1/me"))

    /**
     * cloud.md 5c: a store purchase for the server to verify (402 not confirmed, 409 another account's, 422 a field
     * missing; the same purchase twice gives the same answer). Returns the account with its new plan.
     */
    fun purchase(p: StorePurchase): Account = Account.from(postObject("/v1/me/purchases", buildJsonObject {
        put("store", p.store); put("product_id", p.productId)
        if (p.purchaseToken.isNotBlank()) put("purchase_token", p.purchaseToken)
        if (p.signedTransaction.isNotBlank()) put("signed_transaction", p.signedTransaction)
        if (p.storeIdKey.isNotBlank()) put("store_id_key", p.storeIdKey)
    }))

    /** `GET /v1/me/sessions`: where the account is signed in. */
    fun sessions(): List<AccountSession> = list(get("/v1/me/sessions"), "sessions").map(AccountSession::from)

    /** `DELETE /v1/me/sessions/{id}`. */
    fun closeSession(id: String) {
        parse(send(HttpRequest("DELETE", serverUrl + "/v1/me/sessions/" + path(id), headers(true))))
    }

    /** `POST /v1/me/sessions/close-others`; returns how many were closed. */
    fun closeOtherSessions(): Int = postObject("/v1/me/sessions/close-others", JsonObject(emptyMap())).number("closed").toInt()

    /** `GET /v1/plans`: the public plans; no session needed (rule 5a). */
    fun plans(): List<Plan> = list(get("/v1/plans", auth = token != null), "plans").map(Plan::from)

    // ---- Jobs (rules 6 to 10c) ----

    /**
     * `POST /v1/jobs`: sends a project (a 3MF package) to be sliced or analyzed.
     * [open] gives the project's bytes; it may be called once.
     */
    fun submit(kind: JobKind, params: JsonObject, filename: String, length: Long, open: () -> ByteInput): Job {
        val response = send(HttpRequest(
            "POST", serverUrl + "/v1/jobs", headers(true),
            form = listOf(
                FormPart.text("kind", kind.wire),
                FormPart.text("params", params.toString(), "application/json"),
                FormPart("file", PROJECT_TYPE, filename.ifBlank { "project.3mf" }, length, open),
            ),
            timeoutMs = 30 * 60_000,
        ))
        return Job.from(objectOf(response))
    }

    /** `GET /v1/jobs?limit=N`, the newest first (rule 10). */
    fun jobs(limit: Int = 50): List<Job> =
        list(get("/v1/jobs?limit=${limit.coerceIn(1, 200)}"), "jobs").map(Job::from)

    /** `GET /v1/jobs/{id}`. */
    fun job(id: String): Job = Job.from(getObject("/v1/jobs/" + path(id)))

    /** `POST /v1/jobs/{id}/cancel`. */
    fun cancel(id: String) {
        postObject("/v1/jobs/" + path(id) + "/cancel", JsonObject(emptyMap()))
    }

    /**
     * p2p_library.md, rule 3 bis: the shared library's files that need no session, as raw bytes (their signatures
     * are checked over the exact bytes): `key` (the library key and its certificate), `blocklist.json`,
     * `index.json` and their `.sig`.
     */
    fun libraryFile(name: String, maxBytes: Long = 64L * 1024 * 1024): ByteArray {
        val out = BytesOutput()
        val response = send(HttpRequest("GET", serverUrl + "/v1/library/" + name, headers(false), timeoutMs = 60_000))
        response.use { r ->
            if (!r.ok) throw failure(r)
            val b = ByteArray(64 * 1024)
            var total = 0L
            while (true) {
                val n = r.body.read(b, 0, b.size)
                if (n < 0) break
                total += n
                if (total > maxBytes) throw CloudException(0, "The library's $name is too large")
                out.write(b, 0, n)
            }
        }
        return out.toByteArray()
    }

    /** Rule 7, «Denunciar»: a model or a file, and why; a person reviews it. Returns the report's id. */
    fun libraryReport(model: String?, sha256: String?, reason: String): String =
        postObject("/v1/library/reports", buildJsonObject {
            model?.let { put("model", it) }
            sha256?.let { put("sha256", it) }
            put("reason", reason)
        }).text("id")

    /**
     * p2p_library.md, rule 7 bis: the scanned parts sent to print and not yet taken by [device] (each device takes its
     * own): `GET /v1/me/scans?device=`, `GET /v1/me/scans/{id}/mesh` and `POST /v1/me/scans/{id}/taken {"device"}`.
     */
    fun scans(device: String): List<Scan> =
        list(get("/v1/me/scans?device=" + percentEncode(device)), "scans").map(Scan::from)

    /** A scan's mesh, written to [out], at most [maxBytes]. */
    fun downloadScan(id: String, out: ByteOutput, maxBytes: Long = MAX_RESULT_BYTES): Download =
        fetch("/v1/me/scans/" + path(id) + "/mesh", out, maxBytes)

    /** The scan reached this device's library: the server stops offering it to this device (idempotent). */
    fun scanTaken(id: String, device: String) {
        postObject("/v1/me/scans/" + path(id) + "/taken", buildJsonObject { put("device", device) })
    }

    /**
     * `GET /v1/jobs/{id}/result`, written to [out] as it arrives (rule 9). At
     * most [maxBytes] are read; a larger result is an error.
     */
    fun download(id: String, out: ByteOutput, maxBytes: Long = MAX_RESULT_BYTES): Download =
        fetch("/v1/jobs/" + path(id) + "/result", out, maxBytes)

    private fun fetch(route: String, out: ByteOutput, maxBytes: Long): Download {
        val response = send(HttpRequest("GET", serverUrl + route, headers(true),
            timeoutMs = 30 * 60_000, large = true))
        response.use { r ->
            if (!r.ok) throw failure(r)
            var total = 0L
            val buffer = ByteArray(64 * 1024)
            try {
                while (true) {
                    val n = r.body.read(buffer, 0, buffer.size)
                    if (n < 0) break
                    total += n
                    if (total > maxBytes) throw CloudException(0, "The result is larger than $maxBytes bytes")
                    out.write(buffer, 0, n)
                }
            } catch (e: IOException) {
                throw CloudException(0, e.message ?: "Download interrupted", network = true, cause = e)
            }
            return Download(filenameOf(r.header("Content-Disposition")) ?: "", total)
        }
    }

    /**
     * Follows a job every [intervalMs] (2 s, rule 8) while it is queued or
     * running; the flow ends with the finished job. A lost answer or two are
     * borne; a closed session (401) or ten failures in a row end it with the error.
     */
    fun watch(id: String, intervalMs: Long = 2_000): Flow<Job> = flow {
        var failures = 0
        while (true) {
            val job = try {
                job(id).also { failures = 0 }
            } catch (e: CloudException) {
                if (e.kind == CloudException.Kind.EXPIRED || ++failures >= 10) throw e
                null
            }
            if (job != null) {
                emit(job)
                if (job.status.finished) return@flow
            }
            delay(intervalMs)
        }
    }

    // ---- Problem reports (rule 11) ----

    /** `POST /v1/reports`, with the session when there is one; returns how many earlier reports are alike. */
    fun report(report: ProblemReport): Int {
        val o = postObject("/v1/reports", report.toJson(), auth = token != null)
        return o.number("similar").toInt()
    }

    /** `GET /v1/me/reports`, the newest first. */
    fun myReports(): List<SentReport> = list(get("/v1/me/reports"), "reports").map(SentReport::from)

    // ---- Plumbing ----

    private fun sessionOf(o: JsonObject, email: String): Session {
        val t = o.text("token")
        if (t.isEmpty()) throw CloudException(0, "Unexpected answer of the server")
        token = t
        return Session(t, o.text("email").ifEmpty { email })
    }

    private fun headers(auth: Boolean): Map<String, String> = buildMap {
        put("Accept", "application/json")
        put("User-Agent", USER_AGENT)
        val t = token
        if (auth && t != null) put("Authorization", "Bearer $t")
    }

    private fun send(request: HttpRequest): HttpResponse = try {
        http.send(request)
    } catch (e: IOException) {
        throw CloudException(0, e.message ?: "No answer from the server", network = true, cause = e)
    }

    private fun get(path: String, auth: Boolean = true): JsonElement =
        parse(send(HttpRequest("GET", serverUrl + path, headers(auth))))

    private fun getObject(path: String, auth: Boolean = true): JsonObject = get(path, auth) as? JsonObject
        ?: throw CloudException(0, "Unexpected answer of the server")

    private fun postObject(path: String, body: JsonObject, auth: Boolean = true): JsonObject {
        val h = headers(auth) + ("Content-Type" to "application/json")
        return objectOf(send(HttpRequest("POST", serverUrl + path, h, body.toString().encodeToByteArray())))
    }

    private fun objectOf(r: HttpResponse): JsonObject = parse(r) as? JsonObject
        ?: throw CloudException(0, "Unexpected answer of the server")

    private fun parse(r: HttpResponse): JsonElement {
        val text = try {
            r.text()
        } catch (e: IOException) {
            throw CloudException(0, e.message ?: "Answer interrupted", network = true, cause = e)
        }
        if (!r.ok) throw CloudException(r.status, CloudException.detailOf(text, r.status))
        if (text.isBlank()) return JsonObject(emptyMap())
        return try {
            Json.parseToJsonElement(text)
        } catch (e: Exception) {
            throw CloudException(0, "Unexpected answer of the server", cause = e)
        }
    }

    private fun failure(r: HttpResponse): CloudException {
        val text = runCatching { r.text() }.getOrDefault("")
        return CloudException(r.status, CloudException.detailOf(text, r.status))
    }

    /** A list, or an object that holds it under [key]. */
    private fun list(e: JsonElement, key: String): List<JsonObject> {
        val array = e as? JsonArray ?: (e as? JsonObject)?.get(key) as? JsonArray
            ?: throw CloudException(0, "Unexpected answer of the server")
        return array.filterIsInstance<JsonObject>()
    }

    companion object {
        /** The default server (cloud.md, rule 1). */
        const val DEFAULT_SERVER = "https://tismaservidores.net"

        /** The largest result downloaded (the server keeps them up to 1 GB). */
        const val MAX_RESULT_BYTES = 1L shl 30

        const val PROJECT_TYPE = "application/vnd.ms-package.3dmanufacturing-3dmodel+xml"
        private val USER_AGENT = "Tisma-$PLATFORM_NAME"

        /** Adds `https://` when there is no scheme and drops the trailing slashes. */
        fun normalizeServer(url: String): String {
            val t = url.trim().trimEnd('/')
            if (t.isEmpty()) return DEFAULT_SERVER
            return if (t.startsWith("http://", true) || t.startsWith("https://", true)) t else "https://$t"
        }

        private fun path(id: String) = percentEncode(id)

        /** The file name of a Content-Disposition header, if it has a plain one. */
        internal fun filenameOf(header: String?): String? {
            if (header == null) return null
            val m = Regex("filename=\"?([^\";]+)\"?").find(header) ?: return null
            // Only the last path part, so a name can never point elsewhere.
            return m.groupValues[1].substringAfterLast('/').substringAfterLast('\\').trim().ifEmpty { null }
        }
    }
}

data class Health(val ok: Boolean, val billingUrl: String)

data class Session(val token: String, val email: String)

/** What `POST /v1/jobs` accepts as `kind` (rule 6). */
enum class JobKind(val wire: String) {
    SLICE("slice"), FEA("fea"), AERO("aero"), ORIENT("orient");
}

/** A problem report (rule 11 and problem_reports.md). Empty fields are left out. */
data class ProblemReport(
    val kind: String,
    val title: String,
    val description: String = "",
    val steps: String = "",
    val expected: String = "",
    val actual: String = "",
    /** The version, the system and the log: sent only when the person accepts it. */
    val appVersion: String? = null,
    val os: String? = null,
    val log: String? = null,
) {
    fun toJson(): JsonObject = buildJsonObject {
        put("app", "android")
        put("kind", kind)
        put("title", title)
        listOf("description" to description, "steps" to steps, "expected" to expected, "actual" to actual)
            .forEach { (k, v) -> if (v.isNotEmpty()) put(k, v) }
        appVersion?.let { put("app_version", it) }
        os?.let { put("os", it.take(80)) }
        log?.let { put("log", if (it.length > 190_000) it.takeLast(190_000) else it) }
    }
}
