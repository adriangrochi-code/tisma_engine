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

    /** «Buscar impresoras en la red» (printer_connections.md, «Búsqueda en la red», rule 1). */
    var searchNetwork: Boolean
        get() = prefs.getBoolean(SEARCH_NETWORK, true)
        set(value) = prefs.edit().putBoolean(SEARCH_NETWORK, value).apply()

    /** The printer, nozzle and filament picked from the catalog (mobile_app.md, rule 6: the printer card). */
    var profilePrinter: String?
        get() = prefs.getString("profile.printer", null)
        set(v) = prefs.edit().putString("profile.printer", v).apply()
    var profileNozzle: Double
        get() = prefs.getFloat("profile.nozzle", 0.4f).toDouble()
        set(v) = prefs.edit().putFloat("profile.nozzle", v.toFloat()).apply()
    var profileFilament: String?
        get() = prefs.getString("profile.filament", null)
        set(v) = prefs.edit().putString("profile.filament", v).apply()

    /** The settings panel's view: basic, normal, advanced or custom (settings_panel.md, rule 9; `ui.level`). */
    var panelView: String
        get() = prefs.getString("ui.level", null) ?: "basic"
        set(v) = prefs.edit().putString("ui.level", v).apply()

    /** `ui.custom_visible`: the keys the Custom view shows. */
    var customVisible: Set<String>
        get() = prefs.getStringSet("ui.custom_visible", null)?.toSet() ?: emptySet()
        set(v) = prefs.edit().putStringSet("ui.custom_visible", v).apply()

    /** The settings the person changed over the profile, as a JSON object of `layers.height` keys. */
    var panelChanges: String?
        get() = prefs.getString("panel.changes", null)
        set(v) = prefs.edit().putString("panel.changes", v).apply()

    /** The person's notes on a printer (settings_panel.md, rule 37, «Notas»), by its catalog name. */
    fun printerNotes(printer: String?): String = prefs.getString("printer.notes." + (printer ?: ""), null) ?: ""
    fun setPrinterNotes(printer: String?, text: String) = prefs.edit().putString("printer.notes." + (printer ?: ""), text).apply()

    /** `ui.color_vision` (accessibility_touch.md): normal, deuteranopia, protanopia, tritanopia or achromatopsia. */
    var colorVision: String
        get() = prefs.getString("ui.color_vision", null) ?: "normal"
        set(v) = prefs.edit().putString("ui.color_vision", v).apply()

    /** The printer card's choice is a printer of the profile pack, and its name to show. */
    var profilePack: Boolean
        get() = prefs.getBoolean("profile.pack", false)
        set(v) = prefs.edit().putBoolean("profile.pack", v).apply()
    var profileName: String?
        get() = prefs.getString("profile.name", null)
        set(v) = prefs.edit().putString("profile.name", v).apply()

    /** The first-start wizard ran (mobile_app.md, rule 11; MA1): it is not opened again by itself. */
    var packWizardDone: Boolean
        get() = prefs.getBoolean("pack.wizard_done", false)
        set(v) = prefs.edit().putBoolean("pack.wizard_done", v).apply()

    /** app_updates.md, «Ajustes»: look for new versions on opening (once a day), the channel, download at once. */
    var updatesCheck: Boolean
        get() = prefs.getBoolean("updates.check", true)
        set(v) = prefs.edit().putBoolean("updates.check", v).apply()
    var updatesChannel: String
        get() = prefs.getString("updates.channel", null) ?: "stable"
        set(v) = prefs.edit().putString("updates.channel", v).apply()
    var updatesAutoDownload: Boolean
        get() = prefs.getBoolean("updates.auto_download", true)
        set(v) = prefs.edit().putBoolean("updates.auto_download", v).apply()
    var updatesLastCheck: Long
        get() = prefs.getLong("updates.last_check", 0)
        set(v) = prefs.edit().putLong("updates.last_check", v).apply()

    companion object {
        private const val SEARCH_NETWORK = "printers.search_network"
        const val FILE = "tisma_settings"
        private const val SERVER = "cloud.server"
        private const val EMAIL = "cloud.email"
        private const val CLOUD_PREFER = "cloud.prefer"
    }
}
