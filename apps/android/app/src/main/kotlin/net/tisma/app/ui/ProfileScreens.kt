package net.tisma.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import net.tisma.app.R
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceState
import net.tisma.app.engine.SliceViewModel
import java.util.Locale

/** The printer card on the plate (mobile_app.md, rule 6): the printer, nozzle and filament; a tap changes them. */
@Composable
fun PrinterCard(state: SliceState, onClick: () -> Unit) {
    val p = state.profile
    Card(onClick = onClick, modifier = Modifier.fillMaxWidth().testTag("printer_card")) {
        androidx.compose.foundation.layout.Row(Modifier.padding(16.dp), verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
            androidx.compose.material3.Icon(painterResource(R.drawable.tisma_printer), null, Modifier.padding(end = 12.dp))
            Column(Modifier.weight(1f)) {
                Text(p?.let { it.name ?: it.printer } ?: stringResource(R.string.profile_none), style = MaterialTheme.typography.titleMedium)
                Text(if (p == null) stringResource(R.string.profile_none_hint)
                    else String.format(Locale.getDefault(), "%.2f mm", p.nozzle) + " · " + (p.filament?.let { f -> if (p.pack) f.substringAfterLast('/').removeSuffix(".json").replace('_', ' ') else f }
                        ?: stringResource(if (p.pack) R.string.pack_default_filament else R.string.profile_generic_filament)),
                    style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

/** Picks the printer from the engine's catalog, its nozzle and a filament that fits it (mobile_app.md, rule 11). */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ProfilePickerScreen(state: SliceState, slice: SliceViewModel, nav: Navigator) {
    val current = state.profile
    if (current != null && current.pack) {
        PackFilamentPicker(state, slice, nav, current)
        return
    }
    val catalog = state.catalog
    var query by rememberSaveable { mutableStateOf("") }
    var printer by rememberSaveable { mutableStateOf(state.profile?.printer) }
    var nozzle by rememberSaveable { mutableStateOf(state.profile?.nozzle ?: 0.4) }
    var filament by rememberSaveable { mutableStateOf(state.profile?.filament) }
    if (catalog == null) {
        ScreenColumn { Text(stringResource(R.string.profile_no_catalog)) }
        return
    }
    val chosen = catalog.printer(printer)
    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        if (chosen == null && state.packAvailable) {
            // The profile pack's printers, more than the built-in catalog (profile_pack.md, rule 2).
            androidx.compose.material3.OutlinedButton(onClick = { nav.replace(Screen.PackWizard) }, modifier = Modifier.testTag("profile_pack")) {
                Text(stringResource(R.string.pack_open))
            }
        }
        if (chosen == null) {
            OutlinedTextField(query, { query = it }, singleLine = true, label = { Text(stringResource(R.string.profile_search)) },
                modifier = Modifier.fillMaxWidth().testTag("profile_search"))
            LazyColumn(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                items(catalog.search(query), key = { it.name }) { p ->
                    Card(onClick = { printer = p.name; nozzle = p.nozzles.first(); filament = null },
                        modifier = Modifier.fillMaxWidth().testTag("profile_printer_" + p.name)) {
                        Column(Modifier.padding(12.dp)) {
                            Text(p.name, style = MaterialTheme.typography.bodyLarge)
                            Text(p.buildVolume.joinToString(" × ") { String.format(Locale.getDefault(), "%.0f", it) } + " mm",
                                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            }
        } else {
            Text(chosen.name, style = MaterialTheme.typography.titleLarge)
            AccentTextButton(onClick = { printer = null }) { Text(stringResource(R.string.profile_change_printer)) }
            Text(stringResource(R.string.profile_nozzle), style = MaterialTheme.typography.labelLarge)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                chosen.nozzles.forEach { d ->
                    FilterChip(selected = nozzle == d, onClick = { nozzle = d },
                        label = { Text(String.format(Locale.getDefault(), "%.2f mm", d)) })
                }
            }
            Text(stringResource(R.string.profile_filament), style = MaterialTheme.typography.labelLarge)
            LazyColumn(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                items(catalog.filamentsFor(chosen), key = { it.fullName }) { f ->
                    val sel = filament == f.fullName
                    Card(onClick = { filament = f.fullName }, modifier = Modifier.fillMaxWidth().testTag("profile_filament_" + f.fullName),
                        colors = if (sel) CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.primaryContainer)
                            else CardDefaults.cardColors()) {
                        Text(f.material + " · " + f.fullName, Modifier.padding(12.dp))
                    }
                }
            }
            Button(onClick = { slice.setProfile(EngineRunner.ProfileChoice(chosen.name, nozzle, filament)); nav.back() },
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp).testTag("profile_save")) {
                Text(stringResource(R.string.pref_save))
            }
        }
    }
}

/**
 * The first-start wizard with the free profile pack (mobile_app.md, rule 11;
 * profile_pack.md, rules 1 to 3 and 11; MA1): the index's printers by vendor,
 * searched by name; a model, then its nozzle (the default variant first);
 * «Use this printer» downloads that vendor, installs it and puts it on the
 * printer card, with its default filament; then the filament step (the
 * pack's filaments for that printer). Without network: the built-in catalog.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun PackWizardScreen(state: SliceState, slice: SliceViewModel, nav: Navigator, onDone: () -> Unit) {
    androidx.compose.runtime.LaunchedEffect(Unit) { slice.loadPackIndex() }
    val index = state.packIndex
    var query by rememberSaveable { mutableStateOf("") }
    var chosenFile by rememberSaveable { mutableStateOf<String?>(null) }
    var variantId by rememberSaveable { mutableStateOf<String?>(null) }
    Column(Modifier.fillMaxSize().padding(16.dp).testTag("pack_wizard"), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.pack_title), style = MaterialTheme.typography.titleLarge)
        if (state.packBusy) {
            androidx.compose.material3.LinearProgressIndicator(progress = { state.packProgress / 100f }, modifier = Modifier.fillMaxWidth())
            Text(stringResource(if (index == null) R.string.pack_loading_index else R.string.pack_installing),
                style = MaterialTheme.typography.bodySmall)
        }
        if (index == null && !state.packBusy) {
            // Rule 3: without network, the built-in catalog.
            Text(stringResource(R.string.pack_offline), color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.testTag("pack_offline"))
            Button(onClick = { slice.loadPackIndex() }, modifier = Modifier.testTag("pack_retry")) { Text(stringResource(R.string.pack_retry)) }
        }
        state.packError?.takeIf { index != null }?.let { Text(it, color = MaterialTheme.colorScheme.error) }
        androidx.compose.material3.OutlinedButton(onClick = { onDone(); nav.replace(Screen.ProfilePicker) },
            modifier = Modifier.testTag("pack_builtin")) { Text(stringResource(R.string.pack_builtin)) }
        if (index == null) return@Column
        val found = index.vendors.flatMap { v -> v.models.map { v to it } }.firstOrNull { it.second.file == chosenFile }
        if (found == null) {
            OutlinedTextField(query, { query = it }, singleLine = true, label = { Text(stringResource(R.string.profile_search)) },
                modifier = Modifier.fillMaxWidth().testTag("pack_search"))
            val words = query.trim().lowercase().split(Regex("\\s+")).filter { it.isNotEmpty() }
            val list = index.vendors.flatMap { v -> v.models.map { v to it } }
                .filter { (v, m) -> words.all { w -> m.name.lowercase().contains(w) || v.name.lowercase().contains(w) } }
            LazyColumn(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                items(list.take(300), key = { it.second.file }) { (v, m) ->
                    Card(onClick = { chosenFile = m.file; variantId = m.defaultVariant.ifBlank { m.variants.firstOrNull()?.id } },
                        modifier = Modifier.fillMaxWidth().testTag("pack_model_" + m.name)) {
                        Column(Modifier.padding(12.dp)) {
                            Text(m.name, style = MaterialTheme.typography.titleMedium)
                            Text(v.name, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            }
        } else {
            val (vendor, model) = found
            Text(model.name, style = MaterialTheme.typography.titleMedium)
            Text(stringResource(R.string.profile_nozzle), style = MaterialTheme.typography.labelLarge)
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                model.variants.forEach { va ->
                    FilterChip(selected = variantId == va.id, onClick = { variantId = va.id },
                        label = { Text(if (va.id == String.format(Locale.ROOT, "%.1f", va.nozzle) || va.nozzle == 0.0) va.id
                            else "${va.id} (${va.nozzle} mm)") },
                        modifier = Modifier.testTag("pack_variant_" + va.id))
                }
            }
            AccentTextButton(onClick = { chosenFile = null }) { Text(stringResource(R.string.profile_change_printer)) }
            val variant = model.variants.firstOrNull { it.id == variantId }
            Button(onClick = { if (variant != null) slice.choosePackPrinter(vendor, model, variant) { onDone(); nav.replace(Screen.ProfilePicker) } },
                enabled = variant != null && !state.packBusy, modifier = Modifier.fillMaxWidth().testTag("pack_use")) {
                Text(stringResource(R.string.pack_use))
            }
        }
    }
}

/**
 * A pack printer's filament (profile_pack.md, rules 12 and 13): the
 * printer's default first, then the compatible ones, the printer's vendor's
 * before the generic ones. Another printer is chosen in the wizard.
 */
@Composable
private fun PackFilamentPicker(state: SliceState, slice: SliceViewModel, nav: Navigator, choice: EngineRunner.ProfileChoice) {
    androidx.compose.runtime.LaunchedEffect(choice.printer) { slice.loadPackFilaments() }
    Column(Modifier.fillMaxSize().padding(16.dp).testTag("pack_filaments"), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(choice.name ?: choice.printer, style = MaterialTheme.typography.titleLarge)
        AccentTextButton(onClick = { nav.replace(Screen.PackWizard) }) { Text(stringResource(R.string.profile_change_printer)) }
        Text(stringResource(R.string.profile_filament), style = MaterialTheme.typography.labelLarge)
        LazyColumn(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            item {
                FilterChip(selected = choice.filament == null, onClick = { slice.setProfile(choice.copy(filament = null)); nav.back() },
                    label = { Text(stringResource(R.string.pack_default_filament)) }, modifier = Modifier.testTag("pack_filament_default"))
            }
            items(state.packFilaments, key = { it.path }) { f ->
                Card(onClick = { slice.setProfile(choice.copy(filament = f.path)); nav.back() },
                    modifier = Modifier.fillMaxWidth().testTag("pack_filament_" + f.name),
                    colors = if (choice.filament == f.path) CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.primaryContainer)
                        else CardDefaults.cardColors()) {
                    Column(Modifier.padding(12.dp)) {
                        Text(f.name, style = MaterialTheme.typography.bodyLarge)
                        Text(f.vendor.replace('_', ' '), style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
            }
            if (state.packFilaments.isEmpty()) item {
                Text(stringResource(R.string.pack_no_filaments), color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.testTag("pack_no_filaments"))
            }
        }
    }
}
