package net.tisma.app.printers

import android.content.Context
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.put
import net.tisma.app.data.SecretStore
import net.tisma.printers.PrinterConfig
import net.tisma.printers.PrinterKind
import java.util.UUID

/**
 * The saved printers: name, kind and address in the app's preferences; the
 * key or password in the secret store, encrypted (mobile_app.md, rule 14).
 */
class PrinterStore(context: Context, private val secrets: SecretStore) {
    private val prefs = context.getSharedPreferences(FILE, Context.MODE_PRIVATE)

    fun all(): List<PrinterConfig> {
        val text = prefs.getString(LIST, null) ?: return emptyList()
        val array = runCatching { Json.parseToJsonElement(text) as JsonArray }.getOrNull() ?: return emptyList()
        return array.filterIsInstance<JsonObject>().mapNotNull { o ->
            fun s(k: String) = (o[k] as? JsonPrimitive)?.contentOrNull
            val kind = PrinterKind.of(s("kind") ?: return@mapNotNull null) ?: return@mapNotNull null
            PrinterConfig(s("id") ?: return@mapNotNull null, s("name").orEmpty(), kind, s("url").orEmpty(), s("user").orEmpty())
        }
    }

    fun secret(id: String): String? = secrets.get(secretName(id))

    /** Saves a new printer or replaces one with the same id; a null [secret] keeps the stored one. */
    fun save(config: PrinterConfig, secret: String?): PrinterConfig {
        val list = all().filter { it.id != config.id } + config
        write(list)
        when {
            secret == null -> Unit
            secret.isEmpty() -> secrets.remove(secretName(config.id))
            else -> secrets.put(secretName(config.id), secret)
        }
        return config
    }

    fun remove(id: String) {
        write(all().filter { it.id != id })
        secrets.remove(secretName(id))
    }

    private fun write(list: List<PrinterConfig>) {
        val json = buildJsonArray {
            list.forEach { p ->
                add(buildJsonObject { put("id", p.id); put("name", p.name); put("kind", p.kind.wire); put("url", p.url); if (p.user.isNotEmpty()) put("user", p.user) })
            }
        }
        prefs.edit().putString(LIST, json.toString()).apply()
    }

    companion object {
        const val FILE = "tisma_printers"
        private const val LIST = "printers"
        fun newId(): String = UUID.randomUUID().toString()
        private fun secretName(id: String) = "printer.$id.key"
    }
}
