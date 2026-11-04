package net.tisma.app.ui

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import net.tisma.app.R
import net.tisma.app.engine.PanelVisibility
import net.tisma.app.engine.PanelVisibility.View
import net.tisma.app.engine.SettingsLayout
import net.tisma.app.engine.SliceState
import net.tisma.app.engine.SliceViewModel

/** The four views of the level chip, with their explanation and dot (settings_panel.md, rule 9). */
private data class ViewInfo(val view: View, @StringRes val name: Int, @StringRes val about: Int, val dot: Color)

private val VIEWS = listOf(
    ViewInfo(View.BASIC, R.string.panel_basic, R.string.panel_basic_about, Color(0xFF3DDC97)),
    ViewInfo(View.NORMAL, R.string.panel_normal, R.string.panel_normal_about, Color(0xFFFFD23F)),
    ViewInfo(View.ADVANCED, R.string.panel_advanced, R.string.panel_advanced_about, Color(0xFFE8452C)),
    ViewInfo(View.CUSTOM, R.string.panel_custom, R.string.panel_custom_about, Color(0xFF9B5DE5)),
)

/** The section icons the engine names (`section_*.svg` in assets/icons). */
@DrawableRes
private fun sectionIcon(name: String): Int = when (name.removeSuffix(".svg")) {
    "section_quality" -> R.drawable.section_quality
    "section_strength" -> R.drawable.section_strength
    "section_speed" -> R.drawable.section_speed
    "section_support" -> R.drawable.section_support
    "section_adhesion" -> R.drawable.section_adhesion
    "section_nonplanar" -> R.drawable.section_nonplanar
    "section_temperature" -> R.drawable.section_temperature
    "section_cooling" -> R.drawable.section_cooling
    "section_flow" -> R.drawable.section_flow
    "section_retraction" -> R.drawable.section_retraction
    "section_needs" -> R.drawable.section_needs
    "section_machine" -> R.drawable.section_machine
    "section_nozzle" -> R.drawable.section_nozzle
    "section_limits" -> R.drawable.section_limits
    "section_gcode" -> R.drawable.section_gcode
    else -> R.drawable.section_other
}

private fun SliceState.shows(row: SettingsLayout.Row, section: String) =
    PanelVisibility.visible(row, section, view, custom, changed(row.key)) { value(it) }

/**
 * Print or Material (mobile_app.md, rule 7): the level chip, the search, and
 * the tab's sections; each section opens on its own screen.
 */
@Composable
fun SettingsTabScreen(
    tabId: String, state: SliceState, slice: SliceViewModel, nav: Navigator,
    header: (@Composable () -> Unit)? = null,
    footer: (androidx.compose.foundation.lazy.LazyListScope.() -> Unit)? = null,
) {
    val tab = state.layout?.tab(tabId)
    if (tab == null) {
        Box(Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
            Text(stringResource(R.string.panel_missing))
        }
        return
    }
    var query by rememberSaveable { mutableStateOf("") }
    // Tablets and landscape: the sections on the left, the chosen one on the right (mobile_app.md, rule 5).
    val wide = androidx.compose.ui.platform.LocalConfiguration.current.screenWidthDp >= 720
    var chosen by rememberSaveable(tabId) { mutableStateOf(tab.sections.first().id) }
    if (wide) {
        Row(Modifier.fillMaxSize()) {
            Box(Modifier.weight(1f)) {
                SettingsList(tab, tabId, state, slice, nav, query, { query = it }, header, footer, chosen) { chosen = it }
            }
            Box(Modifier.weight(1.4f)) { SettingsSectionScreen(tabId, chosen, state, slice) }
        }
    } else {
        SettingsList(tab, tabId, state, slice, nav, query, { query = it }, header, footer, null) {
            nav.open(Screen.SettingsSection(tabId, it))
        }
    }
}

@Composable
private fun SettingsList(
    tab: SettingsLayout.Tab, tabId: String, state: SliceState, slice: SliceViewModel, nav: Navigator,
    query: String, setQuery: (String) -> Unit,
    header: (@Composable () -> Unit)?, footer: (androidx.compose.foundation.lazy.LazyListScope.() -> Unit)?,
    chosen: String?, open: (String) -> Unit,
) {
    LazyColumn(Modifier.fillMaxSize().testTag("panel_$tabId"), contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        header?.let { h -> item { h() } }
        item { ViewChips(state, slice, tabId, nav) }
        item {
            OutlinedTextField(query, setQuery, label = { Text(stringResource(R.string.panel_search)) }, singleLine = true,
                modifier = Modifier.fillMaxWidth().testTag("panel_search"))
        }
        val q = query.trim()
        if (q.isNotEmpty()) {
            // Rule 5: a hidden setting is found too, and marked with its level.
            val found = tab.sections.flatMap { s -> s.subsections.flatMap { b -> b.rows.map { s to it } } }
                .filter { (_, r) -> r.label.contains(q, true) || r.key.contains(q, true) }
                .filter { (s, r) -> !PanelVisibility.hiddenByParent(r.key, s.id) { state.value(it) } }
            if (found.isEmpty()) item { Text(stringResource(R.string.panel_nothing_found)) }
            items(found, key = { it.second.key }) { (s, r) ->
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp)) {
                        Text(s.title, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        SettingRow(r, state, slice, marked = !state.shows(r, s.id))
                    }
                }
            }
            return@LazyColumn
        }
        val sections = tab.sections.map { s -> s to s.subsections.sumOf { b -> b.rows.count { state.shows(it, s.id) } } }
            .filter { it.second > 0 }
        if (sections.isEmpty()) {
            item {
                Text(stringResource(if (state.view == View.CUSTOM) R.string.panel_custom_empty else R.string.panel_none),
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        items(sections, key = { it.first.id }) { (s, n) ->
            val changed = s.subsections.any { b -> b.rows.any { state.changed(it.key) } }
            val colors = if (s.id == chosen) androidx.compose.material3.CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.secondaryContainer) else androidx.compose.material3.CardDefaults.cardColors()
            Card(Modifier.fillMaxWidth().clickable { open(s.id) }.testTag("panel_section_${s.id}"), colors = colors) {
                Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                    Icon(painterResource(sectionIcon(s.icon)), null, tint = MaterialTheme.colorScheme.onSurface)
                    Spacer(Modifier.size(12.dp))
                    Column(Modifier.weight(1f)) {
                        Text(s.title, style = MaterialTheme.typography.titleMedium)
                        Text(s.subsections.filter { b -> b.rows.any { state.shows(it, s.id) } }.joinToString(" · ") { it.title },
                            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2)
                    }
                    // Rule 3: a section with changed settings has a violet dot.
                    if (changed) Box(Modifier.size(8.dp).clip(CircleShape).background(Tisma.colors.accentText).testTag("panel_changed_${s.id}"))
                    Spacer(Modifier.size(8.dp))
                    Text(n.toString(), style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        }
        footer?.invoke(this)
        item {
            TextButton(onClick = { slice.resetAll() }, modifier = Modifier.testTag("panel_reset_all")) {
                Text(stringResource(R.string.panel_reset_all))
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun ViewChips(state: SliceState, slice: SliceViewModel, tabId: String, nav: Navigator) {
    Column {
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            VIEWS.forEach { v ->
                FilterChip(selected = state.view == v.view, onClick = { slice.setView(v.view) },
                    leadingIcon = { Box(Modifier.size(8.dp).clip(CircleShape).background(v.dot)) },
                    label = { Text(stringResource(v.name)) }, modifier = Modifier.testTag("panel_view_${v.view.name.lowercase()}"))
            }
        }
        VIEWS.first { it.view == state.view }.let {
            Text(stringResource(it.about), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        if (state.view == View.CUSTOM) {
            OutlinedButton(onClick = { nav.open(Screen.CustomPicker(tabId)) }, modifier = Modifier.padding(top = 8.dp).testTag("panel_choose")) {
                Text(stringResource(R.string.panel_choose))
            }
        }
    }
}

/** One section on its own screen, each subsection as a card (mobile_app.md, rule 7; settings_panel.md, rule 2). */
@Composable
fun SettingsSectionScreen(tabId: String, sectionId: String, state: SliceState, slice: SliceViewModel) {
    val section = state.layout?.tab(tabId)?.sections?.firstOrNull { it.id == sectionId } ?: return
    val subs = section.subsections.map { b -> b to b.rows.filter { state.shows(it, section.id) } }.filter { it.second.isNotEmpty() }
    LazyColumn(Modifier.fillMaxSize().testTag("panel_section"), contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item { Text(section.title, style = MaterialTheme.typography.headlineSmall) }
        items(subs, key = { it.first.id }) { (b, rows) ->
            Card(Modifier.fillMaxWidth().testTag("panel_sub_${b.id}")) {
                Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(b.title, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                        // Rule 8: «Reset» puts the subsection back to the profile.
                        if (rows.any { state.changed(it.key) }) {
                            TextButton(onClick = { rows.forEach { slice.setValue(it.key, null) } }) { Text(stringResource(R.string.panel_reset)) }
                        }
                    }
                    rows.forEach { SettingRow(it, state, slice) }
                }
            }
        }
    }
}

/** A setting: its name, its editor, the arrow back to the profile when changed, and its help. */
@Composable
private fun SettingRow(row: SettingsLayout.Row, state: SliceState, slice: SliceViewModel, marked: Boolean = false) {
    val value = state.value(row.key)
    val changed = state.changed(row.key)
    val levelName = when (row.level) {
        SettingsLayout.Level.BASIC -> null
        SettingsLayout.Level.NORMAL -> R.string.panel_normal
        SettingsLayout.Level.ADVANCED -> R.string.panel_advanced
    }
    Column(Modifier.fillMaxWidth().testTag("row_${row.key}")) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(row.label, style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f),
                color = if (changed) Tisma.colors.accentText else MaterialTheme.colorScheme.onSurface)
            // Rule 12: a changed (or searched) setting the level hides is shown with its level, in gray.
            val hiddenByLevel = when (state.view) {
                View.BASIC -> row.level != SettingsLayout.Level.BASIC
                View.NORMAL -> row.level == SettingsLayout.Level.ADVANCED
                View.ADVANCED -> false
                View.CUSTOM -> row.key !in state.custom
            }
            if ((changed || marked) && hiddenByLevel && levelName != null) {
                Text(stringResource(levelName), style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (changed) {
                TextButton(onClick = { slice.setValue(row.key, null) }, modifier = Modifier.testTag("reset_${row.key}")) { Text("↺") }
            }
        }
        Editor(row, value) { slice.setValue(row.key, it) }
        if (row.help.isNotBlank()) {
            Text(row.help, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 3)
        }
    }
}

@Composable
private fun Editor(row: SettingsLayout.Row, value: kotlinx.serialization.json.JsonElement?, set: (kotlinx.serialization.json.JsonElement) -> Unit) {
    val shown = SettingsLayout.show(value)
    when (val e = row.editor) {
        SettingsLayout.Editor.Toggle -> Switch((value as? JsonPrimitive)?.booleanOrNull == true, { set(JsonPrimitive(it)) },
            modifier = Modifier.testTag("edit_${row.key}").named(row.label))
        is SettingsLayout.Editor.Choice -> {
            var open by remember { mutableStateOf(false) }
            Box {
                OutlinedButton(onClick = { open = true }, modifier = Modifier.testTag("edit_${row.key}")) { Text(shown) }
                DropdownMenu(open, { open = false }) {
                    e.options.forEach { o ->
                        DropdownMenuItem(text = { Text(o) }, onClick = { open = false; set(JsonPrimitive(o)) },
                            modifier = Modifier.testTag("option_${row.key}_$o"))
                    }
                }
            }
        }
        SettingsLayout.Editor.ReadOnly -> Text(stringResource(R.string.panel_on_desktop, shown.take(80)),
            style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        else -> {
            // Typed text is kept here while it does not fit; what fits is set at once.
            var text by remember(row.key, shown) { mutableStateOf(shown) }
            val number = e is SettingsLayout.Editor.Number
            val bad = text.isNotBlank() && SettingsLayout.toJson(e, text) == null
            OutlinedTextField(
                text,
                { t -> text = t; SettingsLayout.toJson(e, t)?.let { if (SettingsLayout.show(it) != shown) set(it) } },
                singleLine = !(e is SettingsLayout.Editor.Text && e.multiline),
                minLines = if (e is SettingsLayout.Editor.Text && e.multiline) 3 else 1,
                isError = bad,
                supportingText = if (bad) ({ Text(stringResource(R.string.panel_invalid, row.range)) }) else null,
                keyboardOptions = KeyboardOptions(keyboardType = if (number) KeyboardType.Decimal else KeyboardType.Text),
                modifier = Modifier.fillMaxWidth().testTag("edit_${row.key}"),
            )
        }
    }
}

/** «Choose settings…» of the Custom view: every setting of the tab, with a check to show it. */
@Composable
fun CustomPickerScreen(tabId: String, state: SliceState, slice: SliceViewModel) {
    val tab = state.layout?.tab(tabId) ?: return
    LazyColumn(Modifier.fillMaxSize().testTag("custom_picker"), contentPadding = androidx.compose.foundation.layout.PaddingValues(16.dp)) {
        tab.sections.forEach { s ->
            item(key = "s_" + s.id) {
                Text(s.title, style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(top = 12.dp, bottom = 4.dp))
            }
            s.subsections.forEach { b ->
                items(b.rows, key = { it.key }) { r ->
                    Row(Modifier.fillMaxWidth().clickable { slice.toggleCustom(r.key) }, verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(r.key in state.custom, { slice.toggleCustom(r.key) }, modifier = Modifier.testTag("pick_${r.key}").named(r.label))
                        Column {
                            Text(r.label)
                            Text(b.title, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            }
        }
    }
}

/**
 * The printer's configuration (mobile_app.md, rule 8; settings_panel.md,
 * rule 37): the printer, nozzle and filament, then Machine, Nozzle and
 * extruder, Limits and G-code as the panel, then the person's notes.
 * Connection is the Printer section itself (the printers on the network).
 */
@Composable
fun PrinterSettingsScreen(state: SliceState, slice: SliceViewModel, nav: Navigator, notes: String, setNotes: (String) -> Unit) {
    SettingsTabScreen("printer", state, slice, nav,
        header = { PrinterCard(state) { nav.open(Screen.ProfilePicker) } },
        footer = {
            item {
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(16.dp)) {
                        Text(stringResource(R.string.printer_calibration), style = MaterialTheme.typography.titleMedium)
                        Text(stringResource(R.string.printer_calibration_later), style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
            }
            item {
                var text by rememberSaveable { mutableStateOf(notes) }
                OutlinedTextField(text, { text = it; setNotes(it) }, label = { Text(stringResource(R.string.printer_notes)) },
                    minLines = 3, modifier = Modifier.fillMaxWidth().testTag("printer_notes"))
            }
        })
}

/** In the Printer section: the printer the plate slices for, and a way into its configuration. */
@Composable
fun PrinterSettingsCard(state: SliceState, nav: Navigator) {
    Card(onClick = { nav.open(Screen.PrinterSettings) }, modifier = Modifier.fillMaxWidth().testTag("printer_settings")) {
        Row(Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(painterResource(R.drawable.section_machine), null, tint = MaterialTheme.colorScheme.onSurface)
            Spacer(Modifier.size(12.dp))
            Column(Modifier.weight(1f)) {
                Text(stringResource(R.string.printer_settings), style = MaterialTheme.typography.titleMedium)
                Text(state.profile?.printer ?: stringResource(R.string.profile_none), style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
