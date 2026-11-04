package net.tisma.app.data

import android.content.Context
import net.tisma.cloud.CloudClient

/** The app's preferences (preferences.md). Nothing secret goes here: that is [SecretStore]. */
class AppSettings(context: Context) {
    private val prefs = context.getSharedPreferences(FILE, Context.MODE_PRIVATE)

    /** The Tisma Cloud server (cloud.md, rule 1). */
    var server: String
        get() = prefs.getString(SERVER, null) ?: CloudClient.DEFAULT_SERVER
        set(value) = prefs.edit().putString(SERVER, CloudClient.normalizeServer(value)).apply()

    /** The e-mail of the open session, to show it; the token is in the secret store. */
    var email: String?
        get() = prefs.getString(EMAIL, null)
        set(value) = prefs.edit().putString(EMAIL, value).apply()

    /** `cloud.prefer`: heavy calculations in the cloud; on by default on the phone (cloud.md, rule 10b). */
    var cloudPrefer: Boolean
        get() = prefs.getBoolean(CLOUD_PREFER, true)
        set(value) = prefs.edit().putBoolean(CLOUD_PREFER, value).apply()

    companion object {
        const val FILE = "tisma_settings"
        private const val SERVER = "cloud.server"
        private const val EMAIL = "cloud.email"
        private const val CLOUD_PREFER = "cloud.prefer"
    }
}
