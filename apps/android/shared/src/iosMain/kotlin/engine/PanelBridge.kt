package net.tisma.app.engine

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull

/** One row of the panel as the SwiftUI list draws it. [kind]: toggle, number, numberOrPercent, choice, text, readOnly. */
data class PanelRow(
    val key: String, val label: String, val help: String, val kind: String, val options: List<String>,
    val value: String, val unit: String, val changed: Boolean,
)

/** A section with the rows to show at the chosen level. */
data class PanelSection(val id: String, val title: String, val rows: List<PanelRow>)

/**
 * The settings panel on the iPhone (settings_panel.md; mobile_app.md, rule 7) from the same layout and rules as
 * Android: tabs, sections, the Basic/Normal/Advanced levels and the switches that hide the rows under them. The
 * person's changes are kept as `layers.height` keys and laid over the printer's profile before slicing.
 */
class PanelBridge(layoutJson: String, changesJson: String) {
    private val layout = SettingsLayout.parse(layoutJson)
    private val changes: MutableMap<String, JsonElement> = runCatching {
        SettingsLayout.flatten(Json.parseToJsonElement(changesJson) as JsonObject).toMutableMap()
    }.getOrDefault(mutableMapOf())

    val tabs: List<String> get() = layout.tabs.map { it.id }

    private fun value(key: String): JsonElement? = changes[key] ?: layout.rows[key]?.let { r ->
        runCatching { Json.parseToJsonElement(r.value.ifBlank { r.default }) }.getOrNull()
    }

    /** The keys the person chose for the Custom view (settings_panel.md, rule 7), kept by the app as one line. */
    private val custom = LinkedHashSet<String>()

    /** The chosen keys as one line (comma separated), to keep between runs; [setCustomLine] reads it back. */
    val customLine: String get() = custom.joinToString(",")

    fun setCustomLine(line: String) { custom.clear(); line.split(',').map { it.trim() }.filter { it in layout.rows }.forEach { custom += it } }

    fun isCustom(key: String): Boolean = key in custom

    fun toggleCustom(key: String) { if (!custom.remove(key) && key in layout.rows) custom += key }

    /**
     * The sections of [tab] with their rows at [level] (0 Basic, 1 Normal, 2 Advanced, 3 Custom: the chosen rows);
     * empty sections left out.
     */
    fun sections(tab: String, level: Int): List<PanelSection> {
        val view = PanelVisibility.View.entries[level.coerceIn(0, 3)]
        val t = layout.tab(tab) ?: return emptyList()
        return t.sections.map { s ->
            PanelSection(s.id, s.title, s.subsections.flatMap { it.rows }.filter { r ->
                PanelVisibility.visible(r, s.id, view, custom, r.key in changes) { value(it) }
            }.map { r -> row(r) })
        }.filter { it.rows.isNotEmpty() }
    }

    /** Every row of [tab] by section, for choosing the Custom view's rows. */
    fun allSections(tab: String): List<PanelSection> {
        val t = layout.tab(tab) ?: return emptyList()
        return t.sections.map { s -> PanelSection(s.id, s.title, s.subsections.flatMap { it.rows }.map { row(it) }) }
            .filter { it.rows.isNotEmpty() }
    }

    private fun row(r: SettingsLayout.Row): PanelRow {
        val (kind, options) = when (val e = r.editor) {
            SettingsLayout.Editor.Toggle -> "toggle" to emptyList()
            is SettingsLayout.Editor.Number -> "number" to emptyList()
            SettingsLayout.Editor.NumberOrPercent -> "numberOrPercent" to emptyList()
            is SettingsLayout.Editor.Choice -> "choice" to e.options
            is SettingsLayout.Editor.Text -> "text" to emptyList()
            SettingsLayout.Editor.ReadOnly -> "readOnly" to emptyList()
        }
        return PanelRow(r.key, r.label, r.help, kind, options, SettingsLayout.show(value(r.key)), r.unit, r.key in changes)
    }

    /** Sets a row from what the person typed (a comma decimal too); false when the text is not a valid value. */
    fun set(key: String, text: String): Boolean {
        val r = layout.rows[key] ?: return false
        val v = SettingsLayout.toJson(r.editor, text) ?: return false
        changes[key] = v
        return true
    }

    fun setToggle(key: String, on: Boolean) { changes[key] = JsonPrimitive(on) }

    /**
     * A calibration's result (`calibration apply`): the settings it changed from [before] become the person's changes,
     * as on Android. How many.
     */
    fun adopt(before: String, applied: String): Int {
        val c = Calibrations.changed(before, applied)
        changes.putAll(c)
        return c.size
    }

    /** Back to the profile's value. */
    fun reset(key: String) { changes.remove(key) }

    fun isOn(key: String): Boolean = (value(key) as? JsonPrimitive)?.booleanOrNull == true

    /** The changes as nested JSON, to keep between runs and to lay over the profile. */
    fun changesJson(): String = SettingsLayout.nest(changes).toString()

    /** The profile's JSON with the changes over it. */
    fun over(profileJson: String): String {
        val base = runCatching { Json.parseToJsonElement(profileJson) as? JsonObject }.getOrNull() ?: JsonObject(emptyMap())
        return PrintChanges.merge(base, SettingsLayout.nest(changes)).toString()
    }
}
