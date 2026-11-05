package net.tisma.app.ui

import android.graphics.BitmapFactory
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import net.tisma.app.R
import net.tisma.app.engine.Library
import net.tisma.app.engine.LibraryViewModel
import net.tisma.app.engine.SliceViewModel
import java.io.File
import java.util.Locale

/**
 * «Mi biblioteca» (local_library.md): the list with its search (rule 4), an
 * entry with its recipes and analyses, open and print (rules 5 and 6), export
 * and share, import (rule 10) and delete with undo (rule 8). Nothing goes to
 * the network.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun LibraryScreen(lib: LibraryViewModel, slice: SliceViewModel, nav: Navigator) {
    val state by lib.state.collectAsStateWithLifecycle()
    val context = LocalContext.current
    LaunchedEffect(Unit) { lib.refresh() }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) copyIn(context, uri)?.let { lib.import(it) }
    }
    // Rule 10: the same file is what Android shares.
    LaunchedEffect(state.exported) {
        state.exported?.let { f ->
            runCatching {
                val uri = androidx.core.content.FileProvider.getUriForFile(context, context.packageName + ".updates", f)
                val send = android.content.Intent(android.content.Intent.ACTION_SEND).setType("application/zip")
                    .putExtra(android.content.Intent.EXTRA_STREAM, uri).addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
                context.startActivity(android.content.Intent.createChooser(send, f.name)
                    .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK))
            }
            lib.sharedExport()
        }
    }
    state.exists?.let { (file, _) ->
        AlertDialog(onDismissRequest = { lib.cancelImport() },
            title = { Text(stringResource(R.string.library_exists)) },
            confirmButton = { TextButton(onClick = { lib.import(file, "replace") }, modifier = Modifier.testTag("library_replace")) {
                Text(stringResource(R.string.library_replace)) } },
            dismissButton = { TextButton(onClick = { lib.import(file, "new") }, modifier = Modifier.testTag("library_keep_both")) {
                Text(stringResource(R.string.library_save_new)) } })
    }
    Column(Modifier.fillMaxSize().padding(16.dp).testTag("library"), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        if (state.busy) LinearProgressIndicator(Modifier.fillMaxWidth())
        state.message?.let { Text(it, color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("library_message")) }
        state.undo?.let {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(R.string.library_deleted), Modifier.weight(1f), style = MaterialTheme.typography.bodySmall)
                TextButton(onClick = { lib.undo() }, modifier = Modifier.testTag("library_undo")) { Text(stringResource(R.string.library_undo)) }
            }
        }
        val entry = state.entry
        if (entry != null) {
            BackHandler { lib.close() }
            EntryDetail(entry, lib, slice, nav)
            return@Column
        }
        OutlinedTextField(state.query, { lib.setQuery(it) }, singleLine = true, label = { Text(stringResource(R.string.profile_search)) },
            modifier = Modifier.fillMaxWidth().testTag("library_search"))
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(state.withAnalysis, { lib.setWithAnalysis(!state.withAnalysis) }, label = { Text(stringResource(R.string.library_with_analysis)) },
                modifier = Modifier.testTag("library_with_analysis"))
            FilterChip(state.byName, { lib.setByName(!state.byName) }, label = { Text(stringResource(R.string.library_by_name)) })
            OutlinedButton(onClick = { pick.launch(arrayOf("*/*")) }, modifier = Modifier.testTag("library_import")) {
                Text(stringResource(R.string.library_import))
            }
        }
        // Rule 12: how much it takes.
        Text(stringResource(R.string.library_size, size(state.totalBytes)), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        val shown = state.shown
        if (shown.isEmpty() && !state.busy) Text(stringResource(if (state.items.isEmpty()) R.string.library_empty else R.string.library_none_found),
            color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.testTag("library_empty"))
        LazyColumn(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            items(shown, key = { it.id }) { i ->
                Card(onClick = { lib.open(i.id) }, modifier = Modifier.fillMaxWidth().testTag("library_item_" + i.name)) {
                    Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                        Thumb(i.thumb)
                        Column(Modifier.weight(1f).padding(start = 12.dp)) {
                            Text(i.name, style = MaterialTheme.typography.titleMedium)
                            val sub = (i.printers + i.materials).joinToString(" · ")
                            if (sub.isNotEmpty()) Text(sub, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                            Text(listOfNotNull(
                                stringResource(R.string.library_recipes, i.recipes).takeIf { i.recipes > 0 },
                                stringResource(R.string.library_analyses, i.analyses).takeIf { i.analyses > 0 },
                                size(i.bytes)).joinToString(" · "),
                                style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun EntryDetail(entry: Library.Entry, lib: LibraryViewModel, slice: SliceViewModel, nav: Navigator) {
    var confirm by remember { mutableStateOf<Triple<String?, String?, String>?>(null) }
    confirm?.let { (recipe, analysis, what) ->
        AlertDialog(onDismissRequest = { confirm = null }, title = { Text(stringResource(R.string.library_delete_ask, what)) },
            confirmButton = { TextButton(onClick = { lib.delete(entry.id, recipe, analysis); confirm = null },
                modifier = Modifier.testTag("library_delete_yes")) { Text(stringResource(R.string.library_delete)) } },
            dismissButton = { TextButton(onClick = { confirm = null }) { Text(stringResource(R.string.cancel)) } })
    }
    LazyColumn(Modifier.fillMaxSize().testTag("library_entry"), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        item {
            Text(entry.name, style = MaterialTheme.typography.titleLarge)
            if (entry.tags.isNotEmpty()) Text(entry.tags.joinToString(", "), style = MaterialTheme.typography.bodySmall)
            if (entry.notes.isNotBlank()) Text(entry.notes, style = MaterialTheme.typography.bodyMedium)
        }
        item { Text(stringResource(R.string.library_recipes_title), style = MaterialTheme.typography.labelLarge) }
        items(entry.recipes, key = { "r" + it.id }) { r ->
            TismaCard(Modifier.testTag("library_recipe_" + r.id)) {
                Text(r.label.ifBlank { r.choice?.name ?: r.choice?.printer ?: "—" }, style = MaterialTheme.typography.titleSmall)
                val sum = listOfNotNull(r.timeS?.let { time(it) }, r.filamentM?.let { String.format(Locale.getDefault(), "%.2f m", it) },
                    r.filamentG?.let { String.format(Locale.getDefault(), "%.1f g", it) },
                    r.layers?.let { stringResource(R.string.library_layers, it) }).joinToString(" · ")
                if (sum.isNotEmpty()) Text(sum, style = MaterialTheme.typography.bodySmall)
                if (r.engine.isNotBlank()) Text(stringResource(R.string.library_engine, r.engine), style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                // Rule 6: always slices with the recipe, here or in Tisma Cloud; the G-code is not kept.
                Button(onClick = { slice.openFromLibrary(entry, r, slice = true); lib.close(); nav.select(Section.PLATE) },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("library_print_" + r.id)) {
                    Text(stringResource(R.string.library_print))
                }
                Row {
                    TextButton(onClick = { slice.openFromLibrary(entry, r); lib.close(); nav.select(Section.PLATE) },
                        modifier = Modifier.testTag("library_open_" + r.id)) { Text(stringResource(R.string.library_open)) }
                    TextButton(onClick = { confirm = Triple(r.id, null, r.label) }) { Text(stringResource(R.string.library_delete)) }
                }
            }
        }
        if (entry.analyses.isNotEmpty()) item { Text(stringResource(R.string.library_analyses_title), style = MaterialTheme.typography.labelLarge) }
        items(entry.analyses, key = { "a" + it.id }) { a ->
            TismaCard(Modifier.testTag("library_analysis_" + a.id)) {
                Text(if (a.kind == "cfd") stringResource(R.string.library_cfd) else stringResource(R.string.library_fea), style = MaterialTheme.typography.titleSmall)
                a.summary.entries.filter { (_, v) -> v is kotlinx.serialization.json.JsonPrimitive }.take(6).forEach { (k, v) ->
                    Text("$k: ${(v as kotlinx.serialization.json.JsonPrimitive).content}", style = MaterialTheme.typography.bodySmall)
                }
                TextButton(onClick = { confirm = Triple(null, a.id, a.kind.uppercase()) }) { Text(stringResource(R.string.library_delete)) }
            }
        }
        item {
            if (entry.recipes.isEmpty()) OutlinedButton(onClick = { slice.openFromLibrary(entry); lib.close(); nav.select(Section.PLATE) },
                modifier = Modifier.fillMaxWidth().testTag("library_open")) { Text(stringResource(R.string.library_open)) }
            OutlinedButton(onClick = { lib.export(entry.id) }, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp).testTag("library_export")) {
                Text(stringResource(R.string.library_export))
            }
            TextButton(onClick = { confirm = Triple(null, null, entry.name) }, modifier = Modifier.testTag("library_delete_entry")) {
                Text(stringResource(R.string.library_delete_entry), color = MaterialTheme.colorScheme.error)
            }
        }
    }
}

@Composable
private fun Thumb(f: File?) {
    val bmp = remember(f) { f?.let { runCatching { BitmapFactory.decodeFile(it.path) }.getOrNull() } }
    if (bmp != null) Image(bmp.asImageBitmap(), null, Modifier.size(56.dp))
    else androidx.compose.material3.Icon(androidx.compose.ui.res.painterResource(R.drawable.tisma_plate), null, Modifier.size(56.dp))
}

private fun size(bytes: Long): String = when {
    bytes >= 1L shl 30 -> String.format(Locale.getDefault(), "%.1f GB", bytes / 1073741824.0)
    bytes >= 1L shl 20 -> String.format(Locale.getDefault(), "%.1f MB", bytes / 1048576.0)
    else -> String.format(Locale.getDefault(), "%d KB", (bytes + 1023) / 1024)
}

private fun time(s: Double): String {
    val m = (s / 60).toInt()
    return if (m >= 60) "${m / 60} h ${m % 60} min" else "$m min"
}

/** A picked `.tismalib` copied where the engine can read it. */
private fun copyIn(context: android.content.Context, uri: Uri): File? = runCatching {
    val name = context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
        if (c.moveToFirst()) c.getString(0) else null
    } ?: "entry.tismalib"
    val dir = File(context.cacheDir, "library/import").apply { deleteRecursively(); mkdirs() }
    val f = File(dir, name.substringAfterLast('/').replace(Regex("[^A-Za-z0-9._ -]"), "_"))
    context.contentResolver.openInputStream(uri)?.use { i -> f.outputStream().use { i.copyTo(it) } } ?: return null
    f
}.getOrNull()
