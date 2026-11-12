package net.tisma.cloud

import net.tisma.app.p2p.foldText

/**
 * What the SwiftUI screens call (apps/ios/PLAN.md, step 3). Kotlin exceptions reach Swift only through `@Throws`,
 * so each call here declares them; Swift sees `try CloudBridge.shared.…`. Calls block: Swift runs them in a task off
 * the main thread.
 */
object CloudBridge {
    /** Throws unless [server] answers `GET /v1/health` as healthy. */
    @Throws(CloudException::class)
    fun checkHealth(server: String) {
        if (!CloudClient(server).health().ok) throw CloudException(0, "The server is not ready")
    }

    /** Signs in and gives the session's token, for the app's keychain. */
    @Throws(CloudException::class)
    fun login(server: String, email: String, password: String, device: String): String =
        CloudClient(server).login(email, password, device).token

    /** The account's plan and hours, as one line for the «Más» screen. */
    @Throws(CloudException::class)
    fun accountLine(server: String, token: String): String {
        val a = CloudClient(server, token).account()
        val hours = if (a.unlimited) "∞" else a.hoursLeft.toString()
        return "${a.email} · ${a.plan} · $hours h"
    }

    /**
     * cloud.md 4a: the notice to show when the service is slow or down («Con demoras: …»), or null when it works or
     * does not say. Never an error: a notice must not get in the way.
     */
    fun serviceNotice(server: String, spanish: Boolean): String? = runCatching { CloudClient(server).status() }.getOrNull()
        ?.takeIf { it.worthNotice }
        ?.let { st -> st.label(spanish) + st.message(spanish).let { m -> if (m.isBlank()) "" else ": $m" } }

    /** The account model of the «Más» screen, on the default server, with Swift's Keychain [tokens]. */
    fun accountModel(tokens: TokenStore): AccountModel = AccountModel(CloudClient.DEFAULT_SERVER, tokens, "iPhone")

    /**
     * cloud.md, rules 6 to 9: sends the 3MF project at [path] to be sliced in the cloud; the job's id. Read from the
     * file in order; a file that does not start like a ZIP is refused here (the server checks the rest).
     */
    @Throws(CloudException::class)
    fun submitSlice(server: String, token: String, path: String, name: String): String {
        val looks = net.tisma.io.FileInput(path).use { looksLikeZip(it) }
        if (!looks) throw CloudException(422, "Not a 3MF project")
        val size = net.tisma.io.FileInput(path).use { measure(it) }
        return CloudClient(server, token).submit(JobRequest.Slice, name, size) { net.tisma.io.FileInput(path) }.id
    }

    /**
     * cloud.md, rule 11 (parity P6): a problem report from the iPhone (`app` `ios`), with the session when there is one
     * ([token] empty: without). [kind] is bug, crash or suggestion; the version and system only when the person
     * accepted to send them. How many earlier reports are alike.
     */
    @Throws(CloudException::class)
    fun report(server: String, token: String, kind: String, title: String, description: String, steps: String, expected: String,
               actual: String, appVersion: String, os: String): Int =
        CloudClient(server, token.ifEmpty { null }).report(ProblemReport(kind, title.trim(), description.trim(), steps.trim(),
            expected.trim(), actual.trim(), appVersion.ifEmpty { null }, os.ifEmpty { null }, null, app = "ios"))

    /** A job's state for the screen: «queued», «running 40 %», «done», or «failed: …» in the phone's language. */
    @Throws(CloudException::class)
    fun jobLine(server: String, token: String, id: String, spanish: Boolean): JobLine {
        val j = CloudClient(server, token).job(id)
        val text = when (j.status) {
            JobStatus.RUNNING -> "${j.status.wire} ${j.progress} %"
            JobStatus.FAILED -> "failed: " + (j.failure()?.message(spanish) { null } ?: j.error)
            else -> j.status.wire
        }
        return JobLine(text, j.status.finished, j.status == JobStatus.DONE)
    }

    /** Downloads a finished slice's G-code to [dest] (through a file on iOS: results go up to 1 GB). */
    @Throws(CloudException::class)
    fun downloadResult(server: String, token: String, id: String, dest: String) {
        val out = net.tisma.io.FileOutput(dest)
        try {
            out.use { CloudClient(server, token).download(id, it) }
        } catch (e: CloudException) {
            platform.posix.remove(dest)
            throw e
        }
    }

    /** The default server (cloud.md, rule 1). */
    val defaultServer: String get() = CloudClient.DEFAULT_SERVER

    /** Search folding, as Android's (accents off, lower case). */
    fun fold(text: String): String = foldText(text)
}

/** A cloud job as the iPhone's list shows it. */
data class JobLine(val text: String, val finished: Boolean, val done: Boolean)
