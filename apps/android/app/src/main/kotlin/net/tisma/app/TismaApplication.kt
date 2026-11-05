package net.tisma.app

import android.app.Application
import android.os.Build
import net.tisma.app.data.AppSettings
import net.tisma.app.data.KeystoreSecretStore
import net.tisma.app.data.SecretStore

/** Holds what lives as long as the app: the preferences and the secret store. */
class TismaApplication : Application() {
    lateinit var settings: AppSettings
        private set
    lateinit var secrets: SecretStore
        private set
    lateinit var crash: net.tisma.app.data.CrashMark
        private set

    override fun onCreate() {
        super.onCreate()
        settings = AppSettings(this)
        secrets = KeystoreSecretStore(this)
        crash = net.tisma.app.data.CrashMark(java.io.File(filesDir, "crash.txt")).also { it.install() }
    }

    /** The device name sent with a login (cloud.md, rule 2), to tell sessions apart. */
    val deviceName: String
        get() = listOf(Build.MANUFACTURER, Build.MODEL).filter { !it.isNullOrBlank() }.distinct()
            .joinToString(" ").ifEmpty { "Android" } + " (Android)"
}
