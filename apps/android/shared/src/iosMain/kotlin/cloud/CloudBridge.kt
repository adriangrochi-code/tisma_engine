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

    /** The default server (cloud.md, rule 1). */
    val defaultServer: String get() = CloudClient.DEFAULT_SERVER

    /** Search folding, as Android's (accents off, lower case). */
    fun fold(text: String): String = foldText(text)
}
