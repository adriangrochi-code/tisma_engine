package net.tisma.mockcloud

import com.sun.net.httpserver.HttpExchange
import com.sun.net.httpserver.HttpServer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.put
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.net.InetSocketAddress
import java.net.URLDecoder
import java.time.Instant
import java.util.UUID
import java.util.concurrent.Executors
import java.util.zip.ZipInputStream

/**
 * A Tisma Cloud test server, written from docs/spec/features/cloud.md (API v1)
 * and nothing else. It keeps everything in memory.
 *
 * - Jobs move one step each time they are asked for (`GET /v1/jobs/{id}`):
 *   queued, running at 30 %, running at 70 %, done (or failed), so tests are
 *   deterministic. [stepsToFinish] changes how many.
 * - The tests change its state through the Kotlin API ([grantHours],
 *   [disable]...); on an emulator the same is done with `POST /mock/...`.
 */
class MockCloud(private val port: Int = 0, private val host: String = "127.0.0.1") {

    data class PlanDef(
        val name: String,
        val monthlyHours: Double,
        val maxConcurrent: Int,
        val priceCents: Long,
        val classes: List<String>,
        val public: Boolean = true,
        /** cloud.md 5c: store → product id. */
        val storeProducts: Map<String, String> = emptyMap(),
    )

    class AccountDef(
        val email: String,
        var password: String,
        var plan: String,
        var verified: Boolean = true,
        var disabled: Boolean = false,
        var grantedHours: Double = 0.0,
        /** cloud.md 5c: store, product id, expiry. */
        var subscription: Triple<String, String, String>? = null,
    )

    /** cloud.md 5c: the store's answer for a proof (true confirmed); and which account each purchase belongs to. */
    @Volatile var storeConfirms: (store: String, proof: String) -> Boolean = { _, proof -> proof.startsWith("ok") }
    private val purchaseOwner = mutableMapOf<String, String>()

    class JobDef(
        val id: String,
        val owner: String,
        val kind: String,
        val jobClass: String,
        val params: JsonObject,
        val createdAt: String,
        var status: String = "queued",
        var progress: Int = 0,
        var seconds: Double = 0.0,
        var steps: Int = 0,
        var startedAt: String? = null,
        var finishedAt: String? = null,
        var error: String? = null,
        var summary: JsonObject = JsonObject(emptyMap()),
        var failure: Failure? = null,
    )

    /** How a job fails: the English text, the stable code and the Spanish text (rule 19a). */
    data class Failure(val error: String, val code: String, val errorEs: String?)

    class ReportDef(val id: Int, val owner: String?, val body: JsonObject, val createdAt: String)

    val plans = mutableListOf(
        PlanDef("Free", 1.0, 1, 0, listOf("slice")),
        PlanDef("Maker", 5.0, 2, 900, listOf("slice", "fea", "orient")),
        PlanDef("Pro", 20.0, 4, 2900, listOf("slice", "slice_large", "fea", "fea_optimize", "aero", "orient"),
            storeProducts = mapOf("google_play" to "pro_month", "app_store" to "net.tismaservidores.tisma.pro_month")),
        PlanDef("Staff", 1000.0, 16, 0, listOf("slice", "slice_large", "fea", "fea_optimize", "aero", "orient"),
            public = false),
    )
    val accounts = mutableMapOf(
        "ana@example.com" to AccountDef("ana@example.com", "password1", "Maker"),
        "free@example.com" to AccountDef("free@example.com", "password1", "Free"),
        "pro@example.com" to AccountDef("pro@example.com", "password1", "Pro"),
    )
    private val tokens = mutableMapOf<String, String>() // token -> email
    private val devices = mutableMapOf<String, Pair<String, String>>() // token -> device, created_at
    private val jobs = mutableListOf<JobDef>()
    val reports = mutableListOf<ReportDef>()
    /** Every request: method, path and the Authorization header, for the tests. */
    val log = mutableListOf<Triple<String, String, String?>>()

    /** The code mailed to confirm an account or reset a password. */
    var mailedCode = "123456"
    var maxUploadBytes = 20L * 1024 * 1024
    var stepsToFinish = 3
    /** When set, the next request gets this status and `detail`. */
    var failNext: Pair<Int, String>? = null
    /** At most this many requests a minute per client before 429; 0 is no limit. */
    var rateLimitPerMinute = 0
    private val recent = ArrayDeque<Long>()
    private var nextJob = 1
    private var nextReport = 1
    private val lock = Any()
    private var server: HttpServer? = null

    val url: String get() = "http://$host:${server!!.address.port}"

    fun start(): MockCloud {
        val s = HttpServer.create(InetSocketAddress(host, port), 0)
        s.executor = Executors.newFixedThreadPool(8)
        s.createContext("/") { ex -> handle(ex) }
        s.start()
        server = s
        return this
    }

    fun stop() {
        server?.stop(0)
        server = null
    }

    // ---- Controls for tests ----

    fun grantHours(email: String, hours: Double) = synchronized(lock) { accounts.getValue(email).grantedHours += hours }
    fun disable(email: String) = synchronized(lock) { accounts.getValue(email).disabled = true }
    fun setPlan(email: String, plan: String) = synchronized(lock) { accounts.getValue(email).plan = plan }
    fun expireTokens() = synchronized(lock) { tokens.clear() }
    fun job(id: String): JobDef? = synchronized(lock) { jobs.firstOrNull { it.id == id } }

    /** p2p_library.md, rule 7 bis: a part scanned and sent to print, for [email]'s devices (the app's proposed routes). */
    class ScanDef(val id: String, val email: String, val name: String, val file: String, val mesh: ByteArray, val takenBy: MutableSet<String> = mutableSetOf())
    private val scans = mutableListOf<ScanDef>()
    /** Tests: how many «taken» calls fail (as a dropped network would), and whether fingerprints come in capitals. */
    @Volatile var failTaken = 0
    @Volatile var upperSha = false
    fun addScan(email: String, name: String, file: String, mesh: ByteArray): ScanDef = synchronized(lock) {
        ScanDef("scan-${scans.size + 1}", email, name, file, mesh).also { scans += it }
    }
    fun scan(id: String): ScanDef? = synchronized(lock) { scans.firstOrNull { it.id == id } }

    // ---- The API ----

    private class Reply(val status: Int, val body: ByteArray, val type: String = "application/json",
                        val headers: Map<String, String> = emptyMap())

    private class Stop(val status: Int, val detail: String) : Exception(detail)

    private fun json(status: Int, e: JsonElement) = Reply(status, e.toString().toByteArray())

    private fun ok(e: JsonElement) = json(200, e)

    private fun handle(ex: HttpExchange) {
        val reply = try {
            synchronized(lock) { log += Triple(ex.requestMethod, ex.requestURI.rawPath, ex.requestHeaders.getFirst("Authorization")) }
            route(ex)
        } catch (s: Stop) {
            json(s.status, buildJsonObject { put("detail", s.detail) })
        } catch (e: Exception) {
            json(500, buildJsonObject { put("detail", "Internal error: ${e.message}") })
        }
        // Whatever was not read is drained, so the client is not cut off.
        runCatching { ex.requestBody.readAllBytes() }
        ex.responseHeaders.add("Content-Type", reply.type)
        reply.headers.forEach { (k, v) -> ex.responseHeaders.add(k, v) }
        ex.sendResponseHeaders(reply.status, if (reply.body.isEmpty()) -1 else reply.body.size.toLong())
        if (reply.body.isNotEmpty()) ex.responseBody.use { it.write(reply.body) } else ex.close()
    }

    private fun route(ex: HttpExchange): Reply {
        val method = ex.requestMethod
        val path = ex.requestURI.rawPath.trimEnd('/')
        val query = parseQuery(ex.requestURI.rawQuery)
        if (path.startsWith("/mock/")) return control(path, readJson(ex))
        synchronized(lock) {
            failNext?.let { (status, detail) -> failNext = null; throw Stop(status, detail) }
            if (rateLimitPerMinute > 0) {
                val now = System.currentTimeMillis()
                while (recent.isNotEmpty() && now - recent.first() > 60_000) recent.removeFirst()
                if (recent.size >= rateLimitPerMinute) throw Stop(429, "Too many requests. Try again later.")
                recent.addLast(now)
            }
        }
        val seg = path.split('/').drop(1) // "v1", ...
        if (seg.firstOrNull() != "v1") throw Stop(404, "Not Found")
        val r = seg.drop(1)
        return when {
            method == "GET" && r == listOf("health") -> ok(buildJsonObject {
                put("ok", true); put("billing_url", "$url/account")
            })
            method == "GET" && r == listOf("plans") -> ok(buildJsonObject {
                put("plans", buildJsonArray { synchronized(lock) { plans.filter { it.public }.forEach { add(planJson(it)) } } })
            })
            method == "POST" && r == listOf("auth", "login") -> login(readJson(ex))
            method == "POST" && r == listOf("auth", "register") -> register(readJson(ex))
            method == "POST" && r == listOf("auth", "verify") -> verify(readJson(ex))
            method == "POST" && r == listOf("auth", "resend") -> { readJson(ex); ok(buildJsonObject { put("ok", true) }) }
            method == "POST" && r == listOf("auth", "password", "forgot") -> { readJson(ex); ok(buildJsonObject { put("ok", true) }) }
            method == "POST" && r == listOf("auth", "password", "reset") -> reset(readJson(ex))
            method == "POST" && r == listOf("auth", "logout") -> {
                val t = bearer(ex); auth(ex); synchronized(lock) { tokens.remove(t) }; ok(buildJsonObject { put("ok", true) })
            }
            method == "GET" && r == listOf("me") -> ok(meJson(auth(ex)))
            method == "POST" && r == listOf("me", "purchases") -> purchase(ex, auth(ex))
            method == "GET" && r == listOf("me", "sessions") -> sessions(ex, auth(ex))
            method == "DELETE" && r.size == 3 && r[0] == "me" && r[1] == "sessions" -> {
                val a = auth(ex)
                synchronized(lock) {
                    val t = tokens.entries.firstOrNull { it.value == a.email && sessionId(it.key) == r[2] }?.key
                        ?: throw Stop(404, "No such session")
                    tokens.remove(t)
                }
                ok(buildJsonObject { put("ok", true) })
            }
            method == "POST" && r == listOf("me", "sessions", "close-others") -> {
                val a = auth(ex)
                val mine = bearer(ex)
                val closed = synchronized(lock) {
                    val others = tokens.filter { it.value == a.email && it.key != mine }.keys
                    others.forEach { tokens.remove(it) }
                    others.size
                }
                ok(buildJsonObject { put("closed", closed) })
            }
            method == "GET" && r == listOf("me", "reports") -> myReports(auth(ex))
            method == "GET" && r == listOf("me", "scans") -> {
                val a = auth(ex)
                val device = query["device"].orEmpty()
                val mine = synchronized(lock) { scans.filter { it.email == a.email && device !in it.takenBy } }
                ok(buildJsonObject {
                    put("scans", kotlinx.serialization.json.buildJsonArray {
                        mine.forEach { sc -> add(buildJsonObject {
                            put("id", sc.id); put("name", sc.name); put("file", sc.file); put("size", sc.mesh.size)
                            put("sha256", java.security.MessageDigest.getInstance("SHA-256").digest(sc.mesh).joinToString("") { "%02x".format(it) }.let { if (upperSha) it.uppercase() else it })
                            put("created", "2026-11-10T00:00:00Z")
                        }) }
                    })
                })
            }
            method == "GET" && r.size == 4 && r[0] == "me" && r[1] == "scans" && r[3] == "mesh" -> {
                val a = auth(ex)
                val sc = scan(r[2])?.takeIf { it.email == a.email } ?: throw Stop(404, "No such scan")
                Reply(200, sc.mesh, "application/octet-stream", mapOf("Content-Disposition" to "attachment; filename=\"${sc.file}\""))
            }
            method == "POST" && r.size == 4 && r[0] == "me" && r[1] == "scans" && r[3] == "taken" -> {
                val a = auth(ex)
                val body = readJson(ex)
                val sc = scan(r[2])?.takeIf { it.email == a.email } ?: throw Stop(404, "No such scan")
                if (failTaken > 0) { failTaken--; throw Stop(503, "Try again") }
                synchronized(lock) { sc.takenBy += (body["device"] as? kotlinx.serialization.json.JsonPrimitive)?.content ?: "" }
                ok(buildJsonObject { put("ok", true) })
            }
            method == "POST" && r == listOf("reports") -> report(ex)
            method == "POST" && r == listOf("jobs") -> submit(ex, auth(ex))
            method == "GET" && r == listOf("jobs") -> listJobs(auth(ex), query["limit"]?.toIntOrNull() ?: 50)
            method == "GET" && r.size == 2 && r[0] == "jobs" -> ok(jobJson(advance(ownJob(auth(ex), r[1]))))
            method == "POST" && r.size == 3 && r[0] == "jobs" && r[2] == "cancel" -> cancel(ownJob(auth(ex), r[1]))
            method == "GET" && r.size == 3 && r[0] == "jobs" && r[2] == "result" -> result(ownJob(auth(ex), r[1]))
            else -> throw Stop(404, "Not Found")
        }
    }

    private fun control(path: String, body: JsonObject): Reply {
        fun s(k: String) = (body[k] as? JsonPrimitive)?.contentOrNull ?: throw Stop(422, "$k is required")
        when (path) {
            "/mock/grant" -> grantHours(s("email"), s("hours").toDouble())
            "/mock/disable" -> disable(s("email"))
            "/mock/plan" -> setPlan(s("email"), s("plan"))
            "/mock/expire" -> expireTokens()
            "/mock/fail-next" -> synchronized(lock) { failNext = s("status").toInt() to s("detail") }
            "/mock/max-upload" -> synchronized(lock) { maxUploadBytes = s("bytes").toLong() }
            else -> throw Stop(404, "Not Found")
        }
        return ok(buildJsonObject { put("ok", true) })
    }

    private fun login(b: JsonObject): Reply {
        val email = field(b, "email").lowercase()
        val password = field(b, "password")
        val device = field(b, "device")
        val a = synchronized(lock) { accounts[email] }
        if (a == null || a.password != password) throw Stop(401, "Wrong e-mail or password")
        if (a.disabled) throw Stop(403, "This account is disabled")
        if (!a.verified) throw Stop(403, "Confirm your e-mail first")
        return ok(session(email, device))
    }

    private fun register(b: JsonObject): Reply {
        val email = field(b, "email").lowercase()
        val password = field(b, "password")
        if (!email.contains('@')) throw Stop(422, "Enter a valid e-mail")
        if (password.length < 8) throw Stop(422, "The password needs 8 or more characters")
        val plan = (b["plan"] as? JsonPrimitive)?.contentOrNull ?: "Free"
        synchronized(lock) {
            if (email in accounts) throw Stop(409, "There is already an account with this e-mail")
            if (plans.none { it.public && it.name == plan }) throw Stop(422, "Unknown plan")
            accounts[email] = AccountDef(email, password, plan, verified = false)
        }
        return ok(buildJsonObject { put("ok", true); put("email", email) })
    }

    private fun verify(b: JsonObject): Reply {
        val email = field(b, "email").lowercase()
        val code = field(b, "code")
        val a = synchronized(lock) { accounts[email] } ?: throw Stop(404, "No account with this e-mail")
        if (code != mailedCode) throw Stop(422, "The code is not right")
        a.verified = true
        return ok(session(email))
    }

    private fun reset(b: JsonObject): Reply {
        val email = field(b, "email").lowercase()
        if (field(b, "code") != mailedCode) throw Stop(422, "The code is not right")
        val password = field(b, "password")
        if (password.length < 8) throw Stop(422, "The password needs 8 or more characters")
        synchronized(lock) { accounts[email]?.password = password }
        return ok(buildJsonObject { put("ok", true) })
    }

    private fun session(email: String, device: String = "unknown"): JsonObject {
        val token = UUID.randomUUID().toString().replace("-", "")
        synchronized(lock) { tokens[token] = email; devices[token] = device to now() }
        return buildJsonObject { put("token", token); put("email", email) }
    }

    private fun bearer(ex: HttpExchange): String? =
        ex.requestHeaders.getFirst("Authorization")?.takeIf { it.startsWith("Bearer ") }?.removePrefix("Bearer ")

    /** The account of the request's session, or 401 (rule 2, CC2, CC15). */
    private fun auth(ex: HttpExchange): AccountDef = synchronized(lock) {
        val email = bearer(ex)?.let { tokens[it] } ?: throw Stop(401, "Not authenticated")
        val a = accounts[email] ?: throw Stop(401, "Not authenticated")
        if (a.disabled) throw Stop(401, "Not authenticated")
        a
    }

    private fun sessionId(token: String) = token.take(8)

    private fun sessions(ex: HttpExchange, a: AccountDef): Reply {
        val mine = bearer(ex)
        return ok(buildJsonObject {
            put("sessions", buildJsonArray {
                synchronized(lock) {
                    tokens.filter { it.value == a.email }.keys.forEach { t ->
                        add(buildJsonObject {
                            put("id", sessionId(t)); put("device", devices[t]?.first ?: "unknown")
                            put("created_at", devices[t]?.second ?: now()); put("last_used", null as String?)
                            put("current", t == mine)
                        })
                    }
                }
            })
        })
    }

    private fun planOf(a: AccountDef) = plans.first { it.name == a.plan }

    private fun planJson(p: PlanDef) = buildJsonObject {
        put("name", p.name)
        put("monthly_hours", p.monthlyHours)
        put("max_concurrent", p.maxConcurrent)
        put("price_cents", p.priceCents)
        put("currency", "usd")
        put("classes", buildJsonArray { p.classes.forEach { add(JsonPrimitive(it)) } })
        if (p.storeProducts.isNotEmpty()) put("store_products", buildJsonObject { p.storeProducts.forEach { (k, v) -> put(k, v) } })
    }

    private fun purchase(ex: HttpExchange, a: AccountDef): Reply {
        val b = readJson(ex)
        fun s(k: String) = (b[k] as? JsonPrimitive)?.content.orEmpty()
        val store = s("store"); val product = s("product_id")
        val proof = when (store) { "google_play" -> s("purchase_token"); "app_store" -> s("signed_transaction"); else -> s("store_id_key") }
        if (store.isEmpty() || product.isEmpty() || proof.isEmpty()) throw Stop(422, "Missing store, product_id or the store's proof")
        val plan = synchronized(lock) { plans.firstOrNull { it.storeProducts[store] == product } } ?: throw Stop(422, "Unknown product")
        if (!storeConfirms(store, proof)) throw Stop(402, "The store did not confirm this purchase")
        synchronized(lock) {
            val owner = purchaseOwner.getOrPut(store + ":" + proof) { a.email }
            if (owner != a.email) throw Stop(409, "This purchase belongs to another account")
            a.plan = plan.name
            a.subscription = Triple(store, product, "2026-12-11T00:00:00Z")
        }
        return ok(meJson(a))
    }

    private fun meJson(a: AccountDef): JsonObject = synchronized(lock) {
        val p = planOf(a)
        val included = p.monthlyHours + a.grantedHours
        val used = jobs.filter { it.owner == a.email }.sumOf { it.seconds } / 3600.0
        buildJsonObject {
            put("email", a.email)
            put("plan", p.name)
            put("paid", p.priceCents > 0)
            put("paid_until", if (p.priceCents > 0) "2026-12-01T00:00:00Z" else null as String?)
            put("hours_included", included)
            put("hours_used", used)
            put("hours_left", (included - used).coerceAtLeast(0.0))
            put("active_jobs", jobs.count { it.owner == a.email && it.status in setOf("queued", "running") })
            put("max_concurrent", p.maxConcurrent)
            put("unlimited", false)
            put("remote_printers", if (p.name == "Pro") 3 else 0)
            put("has_password", true)
            val sub = a.subscription
            if (sub == null) put("subscription", null as String?) else put("subscription", buildJsonObject {
                put("store", sub.first); put("product_id", sub.second); put("expires", sub.third); put("auto_renew", true)
            })
        }
    }

    private fun submit(ex: HttpExchange, a: AccountDef): Reply {
        val type = ex.requestHeaders.getFirst("Content-Type") ?: ""
        val boundary = Regex("boundary=\"?([^\";]+)\"?").find(type)?.groupValues?.get(1)
            ?: throw Stop(422, "Expected multipart/form-data")
        // Like the real server's upload guard: the size is known before reading.
        if (ex.requestHeaders.getFirst("Content-Length") == null) throw Stop(411, "Content-Length is required")
        val body = readLimited(ex, maxUploadBytes + 64 * 1024) ?: throw Stop(413, "The file is too large")
        val parts = parseMultipart(body, boundary)
        val kind = parts["kind"]?.text() ?: throw Stop(422, "kind is required")
        val file = parts["file"]?.data ?: throw Stop(422, "file is required")
        if (file.size > maxUploadBytes) throw Stop(413, "The file is too large")
        val params = parts["params"]?.text()?.takeIf { it.isNotBlank() }?.let {
            runCatching { Json.parseToJsonElement(it) as JsonObject }.getOrNull() ?: throw Stop(422, "params is not JSON")
        } ?: JsonObject(emptyMap())
        if (kind !in setOf("slice", "fea", "aero", "orient")) throw Stop(422, "Unknown kind")
        if (!isThreeMf(file)) throw Stop(422, "The project is not a 3MF file")
        fun p(k: String) = (params[k] as? JsonPrimitive)?.contentOrNull
        val jobClass = when (kind) {
            "slice" -> if (file.size > 10 * 1024 * 1024) "slice_large" else "slice"
            "fea" -> if (p("mode") == "lightest-infill") "fea_optimize" else "fea"
            else -> kind
        }
        if (kind == "aero") {
            val speed = (params["speed"] as? JsonPrimitive)?.doubleOrNull ?: throw Stop(422, "speed is required")
            if (speed <= 0 || speed > 100) throw Stop(422, "speed must be more than 0 and up to 100")
        }
        synchronized(lock) {
            val plan = planOf(a)
            if (jobClass !in plan.classes) {
                val with = plans.filter { it.public && jobClass in it.classes }.joinToString(", ") { it.name }
                throw Stop(402, "Included in the plan $with")
            }
            val active = jobs.count { it.owner == a.email && it.status in setOf("queued", "running") }
            if (active >= plan.maxConcurrent) throw Stop(429, "Too many jobs at once for your plan")
            val job = JobDef(id = "job-${nextJob++}", owner = a.email, kind = kind, jobClass = jobClass,
                params = params, createdAt = now())
            // What the nodes do with jobs they cannot run (CC20).
            if (kind == "fea" && p("mode") == "lightest-infill") {
                job.failure = Failure("The lightest infill search does not run in the cloud yet", "not_in_cloud",
                    "La búsqueda del relleno mínimo todavía no corre en la nube")
            } else if (kind == "fea" && p("mode") !in setOf("analyze", "resin-structure")) {
                job.failure = Failure("Unknown analysis mode", "bad_mode", "Modo de análisis desconocido")
            }
            jobs += job
            return ok(jobJson(job))
        }
    }

    private fun advance(job: JobDef): JobDef = synchronized(lock) {
        if (job.status == "queued" || job.status == "running") {
            job.steps++
            if (job.steps >= stepsToFinish) {
                job.finishedAt = now()
                job.progress = 100
                job.seconds += 12.0
                val f = job.failure
                if (f != null) {
                    job.status = "failed"
                    job.error = f.error
                    job.summary = buildJsonObject {
                        put("error", f.error); put("code", f.code); f.errorEs?.let { put("error_es", it) }
                    }
                } else {
                    job.status = "done"
                    job.summary = summaryFor(job)
                }
            } else {
                if (job.status == "queued") job.startedAt = now()
                job.status = "running"
                job.progress = (100 * job.steps / stepsToFinish).coerceAtMost(99)
                job.seconds += 12.0
            }
        }
        job
    }

    private fun summaryFor(job: JobDef) = when (job.kind) {
        "slice" -> buildJsonObject {
            put("print_time", "0h 18m 42s"); put("print_time_s", 1122); put("filament_mm", 3215.4)
            put("filament_g", 9.59); put("layers", 100)
        }
        "fea" -> buildJsonObject { put("verdict", "holds"); put("safety_factor", 2.4) }
        "aero" -> buildJsonObject {
            put("drag_n", 0.0254); put("cd", 1.05); put("cd_pressure", 0.98); put("cd_friction", 0.07)
            put("reynolds", 13700); put("reynolds_simulated", 13700)
        }
        else -> buildJsonObject { put("cost", 0.42); put("cost_before", 0.9); put("candidates", 64) }
    }

    private fun resultName(job: JobDef) = when (job.kind) {
        "slice" -> "project.gcode"
        "fea" -> "project.tea"
        "aero" -> "project.taero"
        else -> "orientation.json"
    }

    private fun jobJson(j: JobDef) = synchronized(lock) {
        buildJsonObject {
            put("id", j.id)
            put("kind", j.kind)
            put("job_class", j.jobClass)
            put("params", j.params)
            put("status", j.status)
            put("progress", j.progress)
            put("seconds", j.seconds)
            put("error", j.error)
            put("summary", j.summary)
            put("result_filename", if (j.status == "done") resultName(j) else null)
            put("created_at", j.createdAt)
            put("started_at", j.startedAt)
            put("finished_at", j.finishedAt)
        }
    }

    private fun ownJob(a: AccountDef, id: String): JobDef = synchronized(lock) {
        val decoded = URLDecoder.decode(id, "UTF-8")
        jobs.firstOrNull { it.id == decoded && it.owner == a.email } ?: throw Stop(404, "No such job")
    }

    private fun listJobs(a: AccountDef, limit: Int): Reply {
        if (limit !in 1..200) throw Stop(422, "limit must be between 1 and 200")
        return ok(buildJsonObject {
            put("jobs", buildJsonArray {
                synchronized(lock) { jobs.filter { it.owner == a.email }.asReversed().take(limit).forEach { add(jobJson(it)) } }
            })
        })
    }

    private fun cancel(job: JobDef): Reply = synchronized(lock) {
        if (job.status == "queued" || job.status == "running") {
            job.status = "cancelled"
            job.finishedAt = now()
        }
        ok(jobJson(job))
    }

    private fun result(job: JobDef): Reply {
        if (job.status != "done") throw Stop(409, "The job has no result")
        val name = resultName(job)
        val bytes = when (job.kind) {
            "slice" -> sampleGcode().toByteArray()
            "orient" -> """{"rotation": [[1,0,0],[0,0,-1],[0,1,0]], "cost": 0.42, "cost_before": 0.9}""".toByteArray()
            else -> "TISMA-${job.kind.uppercase()}-RESULT\n".toByteArray()
        }
        return Reply(200, bytes, if (job.kind == "orient") "application/json" else "application/octet-stream",
            mapOf("Content-Disposition" to "attachment; filename=\"$name\""))
    }

    private fun report(ex: HttpExchange): Reply {
        val owner = bearer(ex)?.let { synchronized(lock) { tokens[it] } }
        val b = readJson(ex)
        fun s(k: String) = (b[k] as? JsonPrimitive)?.contentOrNull.orEmpty()
        if (s("app") !in setOf("pc", "android", "web")) throw Stop(422, "app must be pc, android or web")
        if (s("kind") !in setOf("bug", "crash", "suggestion")) throw Stop(422, "kind must be bug, crash or suggestion")
        if (s("title").length < 3) throw Stop(422, "The title needs 3 or more characters")
        synchronized(lock) {
            val similar = reports.count { (it.body["title"] as? JsonPrimitive)?.contentOrNull.equals(s("title"), true) }
            reports += ReportDef(nextReport++, owner, b, now())
            return ok(buildJsonObject { put("id", nextReport - 1); put("similar", similar) })
        }
    }

    private fun myReports(a: AccountDef): Reply = ok(buildJsonObject {
        put("reports", buildJsonArray {
            synchronized(lock) {
                reports.filter { it.owner == a.email }.asReversed().take(100).forEach { r ->
                    add(buildJsonObject {
                        put("id", r.id)
                        put("kind", r.body["kind"] ?: JsonPrimitive(""))
                        put("title", r.body["title"] ?: JsonPrimitive(""))
                        put("status", "new")
                        put("category_name", null as String?)
                        put("created_at", r.createdAt)
                        put("updated_at", r.createdAt)
                        put("rewarded_at", null as String?)
                    })
                }
            }
        })
    })

    // ---- Helpers ----

    private fun field(b: JsonObject, k: String): String =
        (b[k] as? JsonPrimitive)?.contentOrNull?.takeIf { it.isNotEmpty() } ?: throw Stop(422, "$k is required")

    private fun readJson(ex: HttpExchange): JsonObject {
        val text = ex.requestBody.readBytes().toString(Charsets.UTF_8)
        if (text.isBlank()) return JsonObject(emptyMap())
        return runCatching { Json.parseToJsonElement(text) as JsonObject }.getOrNull() ?: throw Stop(422, "Body is not JSON")
    }

    /** The body, or null when it is longer than [max] (the rest is drained). */
    private fun readLimited(ex: HttpExchange, max: Long): ByteArray? {
        val out = ByteArrayOutputStream()
        val buf = ByteArray(64 * 1024)
        var total = 0L
        val input = ex.requestBody
        while (true) {
            val n = input.read(buf)
            if (n < 0) break
            total += n
            if (total <= max) out.write(buf, 0, n)
        }
        return if (total > max) null else out.toByteArray()
    }

    class Part(val data: ByteArray) {
        fun text() = data.toString(Charsets.UTF_8)
    }

    private fun parseMultipart(body: ByteArray, boundary: String): Map<String, Part> {
        val delimiter = "--$boundary".toByteArray()
        val parts = mutableMapOf<String, Part>()
        var pos = indexOf(body, delimiter, 0)
        while (pos >= 0) {
            val start = pos + delimiter.size
            if (start + 2 <= body.size && body[start] == '-'.code.toByte() && body[start + 1] == '-'.code.toByte()) break
            val headerEnd = indexOf(body, "\r\n\r\n".toByteArray(), start)
            if (headerEnd < 0) break
            val headers = String(body, start, headerEnd - start, Charsets.UTF_8)
            val next = indexOf(body, ("\r\n--$boundary").toByteArray(), headerEnd + 4)
            if (next < 0) break
            val name = Regex("name=\"([^\"]*)\"").find(headers)?.groupValues?.get(1)
            if (name != null) parts[name] = Part(body.copyOfRange(headerEnd + 4, next))
            pos = next + 2
        }
        return parts
    }

    private fun indexOf(hay: ByteArray, needle: ByteArray, from: Int): Int {
        outer@ for (i in from..hay.size - needle.size) {
            for (j in needle.indices) if (hay[i + j] != needle[j]) continue@outer
            return i
        }
        return -1
    }

    private fun isThreeMf(bytes: ByteArray): Boolean = runCatching {
        ZipInputStream(ByteArrayInputStream(bytes)).use { z ->
            generateSequence { z.nextEntry }.any { it.name.startsWith("3D/") && it.name.endsWith(".model") }
        }
    }.getOrDefault(false)

    private fun parseQuery(q: String?): Map<String, String> = q?.split('&')?.mapNotNull {
        val kv = it.split('=', limit = 2)
        if (kv.size == 2) URLDecoder.decode(kv[0], "UTF-8") to URLDecoder.decode(kv[1], "UTF-8") else null
    }?.toMap() ?: emptyMap()

    private fun now() = Instant.now().toString()

    private fun sampleGcode() = buildString {
        appendLine("; generated by the Tisma Cloud test server")
        appendLine("G28")
        for (layer in 0 until 3) {
            appendLine(";LAYER_CHANGE")
            appendLine("G1 Z${"%.2f".format(java.util.Locale.ROOT, 0.2 * (layer + 1))} F600")
            appendLine("G1 X10 Y10 E1 F1200")
            appendLine("G1 X30 Y10 E2")
        }
        appendLine("; estimated printing time (normal mode) = 18m 42s")
    }
}

