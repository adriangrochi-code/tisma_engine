package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

/**
 * The settings panel of the desktop interface, as `tisma --settings-layout`
 * writes it (mobile_app.md, rule 7; settings_panel.md): tabs, sections,
 * subsections and their rows, each with its key, level, name, help, type,
 * default and range. The app ships it in its assets
 * (tools/settings_layout.sh). How each row is edited comes from its type and
 * range (own design): switches, numbers with their limits, a list of choices,
 * text; lists and objects are shown and kept, not edited on the phone.
 */
class SettingsLayout(val version: String, val tabs: List<Tab>) {
    class Tab(val id: String, val sections: List<Section>)
    class Section(val id: String, val title: String, val icon: String, val subsections: List<Subsection>)
    class Subsection(val id: String, val title: String, val summary: String, val rows: List<Row>)
    class Row(
        val key: String, val level: Level, val label: String, val help: String,
        val type: String, val default: String, val range: String, val value: String,
        val editor: Editor,
        /** The unit to show next to the value (°C), when the layout gives one. */
        val unit: String = "",
        /** The key whose value this row takes while it has none of its own (the first layer's temperatures). */
        val fallback: String? = null,
    )

    enum class Level { BASIC, NORMAL, ADVANCED }

    /** How the phone edits a row. */
    sealed interface Editor {
        data object Toggle : Editor
        data class Number(val integer: Boolean, val min: Double?, val max: Double?) : Editor
        /** A length or a percentage of something ("0.4" or "200%"). */
        data object NumberOrPercent : Editor
        data class Choice(val options: List<String>) : Editor
        data class Text(val multiline: Boolean) : Editor
        /** Lists, objects and the like: shown, edited on the desktop. */
        data object ReadOnly : Editor
    }

    fun tab(id: String) = tabs.firstOrNull { it.id == id }

    /** Every row by key. */
    val rows: Map<String, Row> by lazy {
        buildMap { for (t in tabs) for (s in t.sections) for (b in s.subsections) for (r in b.rows) put(r.key, r) }
    }

    companion object {
        fun parse(text: String): SettingsLayout {
            val root = Json.parseToJsonElement(text).jsonObject
            val raw = root["tabs"]!!.jsonArray.map { it.jsonObject }
            // "the same as top_pattern": the options of a sibling key, so all rows are read first.
            val ranges = HashMap<String, String>()
            for (t in raw) for (s in t.arr("sections")) for (b in s.arr("subsections")) for (r in b.arr("settings"))
                ranges[r.str("key")] = r.str("range")
            val tabs = raw.map { t ->
                Tab(t.str("id"), t.arr("sections").map { s ->
                    Section(s.str("id"), s.str("title"), s.str("icon"), s.arr("subsections").map { b ->
                        Subsection(b.str("id"), b.str("title"), b.str("summary"), b.arr("settings").map { r ->
                            val key = r.str("key")
                            val type = r.str("type")
                            Row(
                                key = key,
                                level = when (r.str("level")) { "basic" -> Level.BASIC; "normal" -> Level.NORMAL; else -> Level.ADVANCED },
                                label = r.str("label").ifBlank { key },
                                help = r.str("help"),
                                type = type,
                                default = unquote(r.str("default")),
                                range = r.str("range"),
                                value = unquote(r.str("value")),
                                editor = editorFor(key, type.ifBlank { guessType(key, unquote(r.str("value"))) },
                                    r.str("range").ifBlank { if (type.isBlank() && key.endsWith("temperature")) "0 a 600" else "" }, ranges)
                                    .let { e -> if (e is Editor.Number) e.copy(min = r.num("min") ?: e.min, max = r.num("max") ?: e.max) else e },
                                unit = r.str("unit"),
                                fallback = r.str("fallback").ifBlank { null },
                            )
                        })
                    })
                })
            }
            return SettingsLayout(root["version"]?.jsonPrimitive?.contentOrNull ?: "", tabs)
        }

        private val CHOICES = Regex("""^[a-z0-9_]+(, [a-z0-9_]+)+$""")
        private val SAME_AS = Regex("""^(?:los mismos que|the same as) ([a-z0-9_.]+)$""")
        private val SPAN = Regex("""^(-?[0-9]+(?:,[0-9]+)?) a (-?[0-9]+(?:,[0-9]+)?)(?: [a-z%/]+)?$""")

        internal fun editorFor(key: String, type: String, range: String, ranges: Map<String, String>): Editor = when (type) {
            "booleano", "sí/no" -> Editor.Toggle
            "número", "entero", "mm", "%" -> {
                val m = SPAN.find(range.trim())
                Editor.Number(type == "entero", m?.groupValues?.get(1)?.toEsDouble(), m?.groupValues?.get(2)?.toEsDouble())
            }
            "número o texto", "número o \"N%\"" -> Editor.NumberOrPercent
            "texto" -> {
                val r = range.trim()
                val same = SAME_AS.find(r)?.groupValues?.get(1)?.let { other ->
                    ranges[if ('.' in other) other else key.substringBeforeLast('.') + "." + other]
                }
                val options = (same ?: r).takeIf { CHOICES.matches(it) }?.split(", ")
                if (options != null) Editor.Choice(options)
                else Editor.Text(multiline = key.contains("gcode") || key.endsWith("_template"))
            }
            else -> Editor.ReadOnly
        }

        /**
         * A row without a documented type (the `machine.variables` ones): what
         * its value looks like, and a temperature (°C, 0 to 600 as
         * `max_nozzle_temp` in docs/CONFIG.md) by its name.
         */
        private fun guessType(key: String, value: String) = when {
            value == "true" || value == "false" -> "booleano"
            value.toDoubleOrNull() != null || key.endsWith("temperature") -> "número"
            else -> "texto"
        }

        private fun String.toEsDouble() = replace(',', '.').toDoubleOrNull()

        private fun unquote(s: String) = if (s.length >= 2 && s.startsWith('"') && s.endsWith('"')) s.substring(1, s.length - 1) else s

        private fun JsonObject.str(k: String) = (this[k] as? JsonPrimitive)?.contentOrNull ?: ""
        private fun JsonObject.num(k: String) = (this[k] as? JsonPrimitive)?.doubleOrNull
        private fun JsonObject.arr(k: String) = (this[k] as? JsonArray)?.map { it.jsonObject } ?: emptyList()

        /**
         * What the person typed as the engine's JSON value for [editor], or null
         * when it does not fit (not a number, outside the range, not a choice).
         */
        fun toJson(editor: Editor, text: String): JsonElement? {
            val t = text.trim()
            return when (editor) {
                Editor.Toggle -> t.toBooleanStrictOrNull()?.let { JsonPrimitive(it) }
                is Editor.Number -> {
                    val v = t.replace(',', '.').toDoubleOrNull() ?: return null
                    if (editor.min != null && v < editor.min - 1e-9) return null
                    if (editor.max != null && v > editor.max + 1e-9) return null
                    if (editor.integer) { if (v % 1.0 != 0.0) null else JsonPrimitive(v.toLong()) } else JsonPrimitive(v)
                }
                Editor.NumberOrPercent -> when {
                    t.endsWith("%") -> t.dropLast(1).trim().replace(',', '.').toDoubleOrNull()?.let { JsonPrimitive(trim(it) + "%") }
                    else -> t.replace(',', '.').toDoubleOrNull()?.let { JsonPrimitive(it) }
                }
                is Editor.Choice -> t.takeIf { it in editor.options }?.let { JsonPrimitive(it) }
                is Editor.Text -> JsonPrimitive(text)
                Editor.ReadOnly -> null
            }
        }

        private fun trim(v: Double) = if (v % 1.0 == 0.0) v.toLong().toString() else v.toString()

        /** A value as the row shows it: numbers without a trailing ".0", text without quotes. */
        fun show(v: JsonElement?): String = when (v) {
            null, JsonNull -> ""
            is JsonPrimitive -> when {
                v.isString -> v.content
                v.booleanOrNull != null -> v.content
                else -> v.doubleOrNull?.let { trim(it) } ?: v.content
            }
            else -> v.toString()
        }

        /** The engine's nested settings (`{"layers": {"height": 0.2}}`) as `layers.height` keys. */
        fun flatten(o: JsonObject, prefix: String = "", out: MutableMap<String, JsonElement> = HashMap()): Map<String, JsonElement> {
            for ((k, v) in o) {
                val key = if (prefix.isEmpty()) k else "$prefix.$k"
                // A row's value can itself be an object (extrusion.retraction): keep both levels.
                out[key] = v
                if (v is JsonObject) flatten(v, key, out)
            }
            return out
        }

        /** `layers.height` keys back to the engine's nested settings. */
        fun nest(values: Map<String, JsonElement>): JsonObject {
            val root = HashMap<String, Any>()
            for ((key, v) in values.entries.sortedBy { it.key }.map { it.key to it.value }) {
                val parts = key.split('.')
                var m = root
                for (p in parts.dropLast(1)) {
                    @Suppress("UNCHECKED_CAST")
                    m = (m.getOrPut(p) { HashMap<String, Any>() } as? HashMap<String, Any>) ?: break
                }
                m[parts.last()] = v
            }
            fun build(m: Map<String, Any>): JsonObject = buildJsonObject {
                for ((k, v) in m) {
                    @Suppress("UNCHECKED_CAST")
                    put(k, if (v is JsonElement) v else build(v as Map<String, Any>))
                }
            }
            return build(root)
        }
    }
}

/**
 * Which rows the panel shows (settings_panel.md, rules 6, 7, 10 and 12): a
 * level shows its rows and those of the levels below; Custom shows the chosen
 * keys; a changed row is always shown; a row that depends on a switch that is
 * off is hidden.
 */
object PanelVisibility {
    enum class View { BASIC, NORMAL, ADVANCED, CUSTOM }

    fun visible(
        row: SettingsLayout.Row, section: String, view: View, custom: Set<String>, changed: Boolean,
        value: (String) -> JsonElement?,
    ): Boolean {
        if (hiddenByParent(row.key, section, value)) return false
        if (changed) return true
        return when (view) {
            View.CUSTOM -> row.key in custom
            View.BASIC -> row.level == SettingsLayout.Level.BASIC
            View.NORMAL -> row.level != SettingsLayout.Level.ADVANCED
            View.ADVANCED -> true
        }
    }

    /**
     * Rule 6: the switches that hide the rows under them when they are off (or
     * on, for `combine_auto`). `support.enabled` hides the rest of the
     * Supports section.
     */
    fun hiddenByParent(key: String, section: String, value: (String) -> JsonElement?): Boolean {
        fun on(k: String) = (value(k) as? JsonPrimitive)?.booleanOrNull == true
        return when {
            (section == "support" || key.startsWith("support.")) && key != "support.enabled" -> !on("support.enabled")
            key.startsWith("zaa.") && key != "zaa.enabled" -> !on("zaa.enabled")
            key == "infill.dense_density" -> !on("infill.dense_under_top")
            key == "infill.combine_layers" -> on("infill.combine_auto")
            else -> false
        }
    }
}
