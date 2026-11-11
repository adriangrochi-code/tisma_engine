package net.tisma.app.cloud

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import net.tisma.app.data.AppLog
import net.tisma.app.data.AppSettings
import net.tisma.app.data.SecretStore
import net.tisma.cloud.Account
import net.tisma.cloud.CloudClient
import net.tisma.cloud.CloudException
import kotlinx.serialization.json.put
import net.tisma.cloud.Job
import net.tisma.cloud.JobRequest
import net.tisma.cloud.Plan
import net.tisma.cloud.ProblemReport
import net.tisma.cloud.SentReport
import net.tisma.cloud.looksLikeZip
import net.tisma.cloud.submit
import java.io.InputStream
import java.io.OutputStream

/** A project picked on the phone, to send to the cloud. */
class ProjectSource(val name: String, val length: Long, val open: () -> InputStream?)

/** What the screens show of Tisma Cloud. */
data class CloudState(
    val server: String,
    val email: String? = null,
    val account: Account? = null,
    val plans: List<Plan> = emptyList(),
    val plansLoaded: Boolean = false,
    val billingUrl: String = "",
    val jobs: List<Job> = emptyList(),
    val jobsLoaded: Boolean = false,
    val reports: List<SentReport> = emptyList(),
    val sessions: List<net.tisma.cloud.AccountSession> = emptyList(),
    /** A request is on its way; the buttons that send wait. */
    val busy: Boolean = false,
    /** Sending a project. */
    val uploading: Boolean = false,
    /** Waiting for the e-mail code after creating an account. */
    val verifying: String? = null,
    val notice: Notice? = null,
) {
    val signedIn get() = email != null
    val currentPlan: Plan? get() = account?.let { a -> plans.firstOrNull { it.name == a.plan } }
}

/** Something to tell the person, shown once. */
sealed interface Notice {
    /** A request failed; the screen words it by its kind (cloud.md, rule 4). */
    data class Failed(val error: CloudException) : Notice
    /** The session ended on the server side (rule 2): sign in again. */
    data object SessionExpired : Notice
    data object NotThreeMf : Notice
    data object CannotRead : Notice
    data object CodeSent : Notice
    data object PasswordChanged : Notice
    data class ReportSent(val similar: Int) : Notice
    data class Saved(val name: String) : Notice
}

/**
 * Tisma Cloud for the screens: the session, the account and plans, the jobs
 * and the reports. Every request runs on [io].
 */
class CloudViewModel(
    private val settings: AppSettings,
    private val secrets: SecretStore,
    private val device: String,
    private val io: CoroutineDispatcher = Dispatchers.IO,
    private val clientFor: (server: String, token: String?) -> CloudClient = { s, t -> CloudClient(s, t) },
    /** How often a running job is asked for (cloud.md, rule 8). */
    private val pollMs: Long = POLL_MS,
    /** Where downloaded results go for the preview. */
    private val cacheDir: java.io.File = java.io.File(System.getProperty("java.io.tmpdir")),
    /** The phone's store, when there is one to buy with (cloud.md 5c). */
    val billing: StoreBilling? = null,
) : ViewModel() {

    private var client = clientFor(settings.server, secrets.get(TOKEN))
    private val _state = MutableStateFlow(
        CloudState(server = client.serverUrl, email = if (client.token != null) settings.email ?: "" else null)
    )
    val state: StateFlow<CloudState> = _state.asStateFlow()

    fun dismissNotice() = _state.update { it.copy(notice = null) }

    fun setServer(url: String) {
        val normalized = CloudClient.normalizeServer(url)
        if (normalized == client.serverUrl) return
        // A token belongs to its server: changing it closes the session here.
        forgetSession()
        settings.server = normalized
        client = clientFor(normalized, null)
        _state.update { CloudState(server = normalized) }
        refreshPublic()
    }

    fun setCloudPrefer(on: Boolean) {
        settings.cloudPrefer = on
    }

    /** The public plans and the billing page; no session needed (rule 5a). */
    fun refreshPublic() = launchRequest(quiet = true) {
        val plans = client.plans()
        val billing = runCatching { client.health().billingUrl }.getOrDefault("")
        _state.update { it.copy(plans = plans, plansLoaded = true, billingUrl = billing.ifEmpty { it.billingUrl }) }
    }

    /** Plans and, with a session, the account (asked again each time the panel opens, rule 5a). */
    fun refreshAccount() {
        refreshPublic()
        if (!state.value.signedIn) return
        if (settings.pendingPurchases.isNotEmpty()) restorePurchases()
        launchRequest(quiet = true) {
            val account = client.account()
            settings.email = account.email
            _state.update { it.copy(account = account, email = account.email) }
        }
    }

    fun signIn(email: String, password: String) = launchRequest {
        keep(client.login(email.trim(), password, device))
        AppLog.line("cloud: signed in")
        refreshAccount()
        restorePurchases()
    }

    /** cloud.md 5c: buys the plan's product in the phone's store and has the server verify it. */
    fun buy(plan: net.tisma.cloud.Plan) {
        val b = billing ?: return
        val product = plan.storeProducts[b.store] ?: return
        viewModelScope.launch {
            // The store's proof must name this account (df50c4e): without the token, no purchase is started.
            val token = state.value.account?.purchaseAccountToken?.takeIf { it.isNotBlank() } ?: return@launch
            val proof = runCatching { b.buy(product, token) }.onFailure { AppLog.line("store: ${it.message}") }.getOrNull() ?: return@launch
            send(proof)
        }
    }

    /**
     * Sends a purchase to the server. When the store does not answer it (503), the purchase is kept on the phone and
     * sent again at the next sign-in or account refresh: a paid plan is never lost to a passing outage.
     */
    private suspend fun send(p: net.tisma.cloud.StorePurchase, quiet: Boolean = false): net.tisma.cloud.Account? {
        val line = purchaseLine(p)
        val r = try {
            withContext(io) { client.purchase(p) }
        } catch (e: CloudException) {
            if (e.status == 503 || e.network) { settings.pendingPurchases += line; AppLog.line("store: kept for later (${e.status})") }
            else { settings.pendingPurchases -= line; if (!quiet) _state.update { it.copy(notice = Notice.Failed(e)) } }
            return null
        }
        settings.pendingPurchases -= line
        _state.update { it.copy(account = r) }
        return r
    }

    /** A kept purchase belongs to the account it was bought with: never sent from another one signed in later. */
    private fun purchaseLine(p: net.tisma.cloud.StorePurchase) = kotlinx.serialization.json.buildJsonObject {
        put("account", state.value.email ?: settings.email ?: ""); put("store", p.store); put("product_id", p.productId); put("purchase_token", p.purchaseToken)
        put("signed_transaction", p.signedTransaction); put("store_id_key", p.storeIdKey)
    }.toString()

    private fun accountOf(line: String): String? = runCatching {
        ((kotlinx.serialization.json.Json.parseToJsonElement(line) as kotlinx.serialization.json.JsonObject)["account"]
            as? kotlinx.serialization.json.JsonPrimitive)?.content
    }.getOrNull()

    private fun purchaseOf(line: String): net.tisma.cloud.StorePurchase? = runCatching {
        val o = kotlinx.serialization.json.Json.parseToJsonElement(line) as kotlinx.serialization.json.JsonObject
        fun s(k: String) = (o[k] as? kotlinx.serialization.json.JsonPrimitive)?.content.orEmpty()
        net.tisma.cloud.StorePurchase(s("store"), s("product_id"), s("purchase_token"), s("signed_transaction"), s("store_id_key"))
    }.getOrNull()

    /** «Restaurar compras»: the store's active purchases go to the server again, so a new phone gets the plan. */
    private fun restorePurchases() {
        val b = billing
        viewModelScope.launch {
            val me = state.value.email ?: settings.email
            val kept = settings.pendingPurchases.filter { accountOf(it) == me }.mapNotNull { purchaseOf(it) }
            val active = b?.let { runCatching { it.active() }.getOrDefault(emptyList()) }.orEmpty()
            for (p in (kept + active).distinct()) send(p, quiet = true)
        }
    }

    fun register(email: String, password: String, plan: String?) = launchRequest {
        client.register(email.trim(), password, plan)
        _state.update { it.copy(verifying = email.trim(), notice = Notice.CodeSent) }
    }

    fun verify(code: String) = launchRequest {
        val email = state.value.verifying ?: return@launchRequest
        keep(client.verify(email, code.trim(), device))
        _state.update { it.copy(verifying = null) }
        refreshAccount()
    }

    fun cancelVerify() = _state.update { it.copy(verifying = null) }

    fun resendCode() = launchRequest {
        state.value.verifying?.let { client.resendCode(it) }
        _state.update { it.copy(notice = Notice.CodeSent) }
    }

    fun forgotPassword(email: String, onSent: () -> Unit) = launchRequest(onDone = onSent) {
        client.forgotPassword(email.trim())
        _state.update { it.copy(notice = Notice.CodeSent) }
    }

    fun resetPassword(email: String, code: String, password: String, onDone: () -> Unit) =
        launchRequest(onDone = onDone) {
            client.resetPassword(email.trim(), code.trim(), password)
            _state.update { it.copy(notice = Notice.PasswordChanged) }
        }

    fun signOut() = launchRequest(quiet = true) {
        try {
            client.logout()
        } finally {
            forgetSession()
            _state.update { it.copy(email = null, account = null, jobs = emptyList(), jobsLoaded = false, reports = emptyList()) }
        }
    }

    /** Asks for the list again every 2 s while a job is queued or running (rule 8); call it from the screen. */
    suspend fun followJobs() {
        while (currentCoroutineContextActive()) {
            if (!state.value.signedIn) return
            val jobs = poll { client.jobs() } ?: return
            _state.update { it.copy(jobs = jobs, jobsLoaded = true) }
            if (jobs.none { !it.status.finished }) return
            delay(pollMs)
        }
    }

    /** Follows one job until it ends (rule 8). */
    suspend fun followJob(id: String) {
        while (currentCoroutineContextActive()) {
            val job = poll { client.job(id) } ?: return
            _state.update { s ->
                val others = s.jobs.filter { it.id != id }
                s.copy(jobs = (listOf(job) + others).sortedByDescending { it.createdAt })
            }
            if (job.status.finished) return
            delay(pollMs)
        }
    }

    /** Sends a project; [onSent] gets the new job's id. */
    fun submit(request: JobRequest, project: ProjectSource, onSent: (String) -> Unit) {
        if (state.value.uploading) return
        _state.update { it.copy(uploading = true) }
        viewModelScope.launch {
            try {
                val readable = withContext(io) { runCatching { project.open()?.use { looksLikeZip(it) } }.getOrNull() }
                when (readable) {
                    null -> {
                        AppLog.line("cloud: cannot read ${project.name}")
                        _state.update { it.copy(notice = Notice.CannotRead) }
                    }
                    false -> {
                        AppLog.line("cloud: ${project.name} is not a ZIP")
                        _state.update { it.copy(notice = Notice.NotThreeMf) }
                    }
                    true -> {
                        val job = request {
                            client.submit(request, project.name, project.length) {
                                project.open() ?: throw java.io.IOException("Cannot read ${project.name}")
                            }
                        }
                        if (job != null) {
                            AppLog.line("cloud: job ${job.id} sent (${job.kind})")
                            _state.update { it.copy(jobs = listOf(job) + it.jobs.filter { j -> j.id != job.id }) }
                            onSent(job.id)
                        }
                    }
                }
            } finally {
                _state.update { it.copy(uploading = false) }
            }
        }
    }

    fun cancel(id: String) = launchRequest {
        client.cancel(id)
        val job = client.job(id)
        _state.update { s -> s.copy(jobs = s.jobs.map { if (it.id == id) job else it }) }
    }

    /** Writes a job's result where the person chose (rule 9). */
    fun saveResult(id: String, name: String, open: () -> OutputStream?) = launchRequest {
        val out = open() ?: run {
            _state.update { it.copy(notice = Notice.CannotRead) }
            return@launchRequest
        }
        out.use { client.download(id, it) }
        _state.update { it.copy(notice = Notice.Saved(name)) }
    }

    /** Downloads a slice's G-code to the cache and gives its path, for the preview (rule 9, MA10). */
    fun previewResult(id: String, onReady: (String) -> Unit) {
        var path: String? = null
        launchRequest(onDone = { path?.let(onReady) }) {
            val dir = java.io.File(cacheDir, "results").apply { mkdirs() }
            val f = java.io.File(dir, "job-" + id.filter { it.isLetterOrDigit() || it == '-' } + ".gcode")
            // Only the result on screen is kept: results go up to 1 GB, and the cache is not emptied on its own.
            dir.listFiles()?.filter { it != f }?.forEach { it.delete() }
            try {
                f.outputStream().use { client.download(id, it) }
            } catch (e: Exception) {
                f.delete() // a cut download is not left half there
                throw e
            }
            path = f.path
        }
    }

    /** Sends a problem report (rule 11; problem_reports.md, rules 6 to 8). */
    fun sendReport(report: ProblemReport, onSent: () -> Unit) = launchRequest(onDone = onSent) {
        val similar = client.report(report)
        _state.update { it.copy(notice = Notice.ReportSent(similar)) }
    }

    /** The account's open sessions (cloud.md, «La cuenta»). */
    fun loadSessions() = launchRequest(quiet = true) {
        val list = client.sessions()
        _state.update { it.copy(sessions = list) }
    }

    fun closeSession(id: String) = launchRequest {
        client.closeSession(id)
        val list = client.sessions()
        _state.update { it.copy(sessions = list) }
    }

    fun closeOtherSessions() = launchRequest {
        client.closeOtherSessions()
        val list = client.sessions()
        _state.update { it.copy(sessions = list) }
    }

    fun loadMyReports() = launchRequest(quiet = true) {
        val reports = client.myReports()
        _state.update { it.copy(reports = reports) }
    }

    // ---- Plumbing ----

    private fun keep(session: net.tisma.cloud.Session) {
        secrets.put(TOKEN, session.token)
        settings.email = session.email
        _state.update { it.copy(email = session.email) }
    }

    private fun forgetSession() {
        client.token = null
        secrets.remove(TOKEN)
        settings.email = null
    }

    private fun expired() {
        forgetSession()
        _state.update {
            it.copy(email = null, account = null, jobs = emptyList(), jobsLoaded = false, reports = emptyList(),
                notice = Notice.SessionExpired)
        }
    }

    /** Runs [block] on [io]; a failure becomes a notice (unless [quiet] and not an expired session). */
    private suspend fun <T> request(quiet: Boolean = false, block: () -> T): T? = try {
        withContext(io) { block() }
    } catch (e: CloudException) {
        AppLog.line("cloud: ${e.kind.name.lowercase()} ${e.status} ${e.detail}")
        if (e.kind == CloudException.Kind.EXPIRED && client.token != null) expired()
        else if (!quiet) _state.update { it.copy(notice = Notice.Failed(e)) }
        null
    }

    /**
     * A request made while following a job: without network (the phone in a lift, Wi-Fi to mobile data) it waits and
     * asks again, longer each time up to 30 s, without a message each time; any other failure ends the following.
     * Before, one lost answer stopped following the job for good, with its progress frozen on screen.
     */
    private suspend fun <T> poll(block: () -> T): T? {
        var wait = pollMs
        while (currentCoroutineContextActive()) {
            try {
                return withContext(io) { block() }
            } catch (e: CloudException) {
                if (!e.network) return request { throw e }
                AppLog.line("cloud: no answer while following, again in ${wait} ms")
                delay(wait)
                wait = (wait * 2).coerceAtMost(maxOf(pollMs, 30_000L))
            }
        }
        return null
    }

    /** Runs the blocking [block] off the main thread; [onDone] runs on the main thread if it did not fail. */
    private fun launchRequest(quiet: Boolean = false, onDone: (() -> Unit)? = null, block: () -> Unit) =
        viewModelScope.launch {
            _state.update { it.copy(busy = true) }
            val ok = try {
                request(quiet) { block(); true } ?: false
            } finally {
                _state.update { it.copy(busy = false) }
            }
            if (ok) onDone?.invoke()
        }

    private suspend fun currentCoroutineContextActive() = kotlinx.coroutines.currentCoroutineContext().isActive

    companion object {
        const val TOKEN = "cloud.token"
        const val POLL_MS = 2_000L
    }

    /**
     * p2p_library.md, rule 7 bis: the parts scanned with Tisma Scanner and sent to print reach this device. Each mesh
     * is downloaded (checked against its SHA-256 when the server gives it), handed to [keep] (it goes into «Mi
     * biblioteca»), and only then marked taken. Returns how many arrived. Signed out, or with no answer, none.
     */
    suspend fun takeScans(dir: java.io.File, keep: (net.tisma.cloud.Scan, java.io.File) -> Boolean): Int {
        if (!state.value.signedIn) return 0
        return withContext(io) {
            val list = runCatching { client.scans(device) }.onFailure { AppLog.line("cloud: scans ${it.message}") }.getOrDefault(emptyList())
            var n = 0
            for (scan in list) {
                // Kept before, but the server did not hear «taken»: only tell it again.
                if (scan.id in settings.scansKept) {
                    runCatching { client.scanTaken(scan.id, device) }.onSuccess { settings.scansKept -= scan.id }
                    continue
                }
                var f: java.io.File? = null
                runCatching {
                    dir.mkdirs()
                    val ext = scan.file.substringAfterLast('.', "stl").lowercase().takeIf { it in setOf("stl", "obj", "3mf") } ?: "stl"
                    val file = java.io.File(dir, "scan-" + scan.id.replace(Regex("[^A-Za-z0-9_-]"), "_") + "." + ext).also { f = it }
                    file.outputStream().use { client.downloadScan(scan.id, it) }
                    // The fingerprint in hex, whatever its case.
                    if (scan.sha256.isNotBlank() && !net.tisma.app.p2p.sha256(file).equals(scan.sha256.trim(), ignoreCase = true)) {
                        throw java.io.IOException("The scan's mesh does not match its fingerprint")
                    }
                    // Rule 3 octies: a mesh that fails the safety checks is not kept.
                    if (net.tisma.app.p2p.SafeFile.check(file) is net.tisma.app.p2p.SafeFile.Verdict.Unsafe) {
                        throw java.io.IOException("Unsafe file: not opened")
                    }
                    if (keep(scan, file)) {
                        n++
                        settings.scansKept += scan.id
                        client.scanTaken(scan.id, device)
                        settings.scansKept -= scan.id
                    }
                }.onFailure { AppLog.line("cloud: scan ${scan.id} ${it.message}") }
                // A half download or a refused file never stays behind.
                f?.delete()
            }
            n
        }
    }
}


/**
 * cloud.md 5c: the phone's store. The purchase itself needs the store's library (Google Play Billing), which waits for
 * the owner's decision on its licence; until then there is none and plans are not offered for purchase in the app.
 */
interface StoreBilling {
    /** `google_play`, `app_store` or `microsoft_store`. */
    val store: String
    /**
     * Buys [productId] in the store's own sheet, tied to [accountToken] (the account's `purchase_account_token`, so the
     * store's proof names the account); the proof, or null when the person cancelled.
     */
    suspend fun buy(productId: String, accountToken: String): net.tisma.cloud.StorePurchase?
    /** The store's active purchases of this app, sent again at sign-in («restaurar compras»). */
    suspend fun active(): List<net.tisma.cloud.StorePurchase>
}
