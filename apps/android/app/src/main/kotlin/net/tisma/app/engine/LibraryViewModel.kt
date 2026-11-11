package net.tisma.app.engine

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import net.tisma.app.data.AppLog
import java.io.File

/** The screen of «Mi biblioteca» (local_library.md, rules 4, 8 and 10). */
data class LibraryState(
    val items: List<Library.Item> = emptyList(),
    val query: String = "",
    val withAnalysis: Boolean = false,
    val byName: Boolean = false,
    val entry: Library.Entry? = null,
    val busy: Boolean = false,
    val message: String? = null,
    /** Rule 10: an imported entry that is already there, to replace or keep both. */
    val exists: Pair<File, String>? = null,
    /** Rule 8: the last thing deleted, which can be put back until the app closes. */
    val undo: Deleted? = null,
    /** An exported `.tismalib`, to share. */
    val exported: File? = null,
) {
    /** Rule 4: by name, tag, printer or material, «with analysis»; newest first or by name. */
    val shown: List<Library.Item>
        get() {
            val words = searchWords(query)
            return items.filter { i ->
                (!withAnalysis || i.analyses > 0) && words.all { w ->
                    fold(i.name).contains(w) || (i.tags + i.printers + i.materials).any { fold(it).contains(w) }
                }
            }.let { l -> if (byName) l.sortedBy { it.name.lowercase() } else l.sortedByDescending { it.modified } }
        }

    val totalBytes: Long get() = items.sumOf { it.bytes }
}

/** What was deleted (an entry, a recipe or an analysis) and the entry's copy to put it back. */
data class Deleted(val entry: String, val what: String, val copy: File)

class LibraryViewModel(
    private val library: Library,
    private val workDir: File,
    private val io: CoroutineDispatcher = Dispatchers.IO,
) : ViewModel() {
    private val _state = MutableStateFlow(LibraryState())
    val state: StateFlow<LibraryState> = _state.asStateFlow()

    init {
        // Rule 8: undo lasts until the app closes. When Android ends the app, onCleared never runs: the copies of a
        // past run are removed here, so they do not pile up on the phone.
        File(workDir, "undo").deleteRecursively()
    }

    fun setQuery(q: String) = _state.update { it.copy(query = q) }
    fun setWithAnalysis(v: Boolean) = _state.update { it.copy(withAnalysis = v) }
    fun setByName(v: Boolean) = _state.update { it.copy(byName = v) }
    fun dismissMessage() = _state.update { it.copy(message = null) }
    fun sharedExport() = _state.update { it.copy(exported = null) }

    fun refresh() = work { val l = library.list(); _state.update { it.copy(items = l) } }

    fun open(id: String) = work { val e = library.show(id); _state.update { it.copy(entry = e) } }

    fun close() = _state.update { it.copy(entry = null) }

    /** Rule 10: one file with the entry inside, to share or keep. */
    fun export(id: String) = work {
        val dir = File(workDir, "export").apply { deleteRecursively(); mkdirs() }
        val name = (_state.value.entry?.name ?: id).replace(Regex("[^A-Za-z0-9._ -]"), "_").ifBlank { id }
        val f = library.export(id, File(dir, "$name.tismalib"))
        _state.update { it.copy(exported = f) }
    }

    /** Rule 10: the engine checks the file first (ML6); one already there asks «Reemplazar» or «Guardar como nueva». */
    fun import(file: File, mode: String? = null) = work {
        when (val r = library.import(file, mode)) {
            is Library.Imported.Done -> {
                _state.update { it.copy(exists = null, entry = r.entry, message = null) }
                val l = library.list(); _state.update { it.copy(items = l) }
            }
            is Library.Imported.Exists -> _state.update { it.copy(exists = file to r.id) }
        }
    }

    fun cancelImport() = _state.update { it.copy(exists = null) }

    /** Rule 1, «La imprimí»: today, with an optional photo and a note. */
    fun printed(id: String, photo: File?, note: String) = work {
        val date = java.time.LocalDate.now().toString()
        val e = library.printed(id, date, photo, note)
        val l = library.list()
        _state.update { it.copy(entry = e, items = l) }
    }

    /** Rule 8: deletes an entry, a recipe or an analysis; a copy is kept to undo it until the app closes. */
    fun delete(id: String, recipe: String? = null, analysis: String? = null) = work {
        val copy = library.export(id, File(File(workDir, "undo").apply { mkdirs() }, "$id-${System.nanoTime()}.tismalib"))
        // Only the last deletion can be undone: the copy before it goes.
        _state.value.undo?.copy?.delete()
        library.delete(id, recipe, analysis)
        val what = when { recipe != null -> "recipe"; analysis != null -> "analysis"; else -> "entry" }
        val entry = if (what == "entry") null else library.show(id)
        val l = library.list()
        _state.update { it.copy(items = l, entry = entry, undo = Deleted(id, what, copy)) }
    }

    fun undo() = work {
        val d = _state.value.undo ?: return@work
        library.import(d.copy, "replace")
        d.copy.delete()
        val l = library.list()
        _state.update { it.copy(items = l, undo = null, entry = if (d.what == "entry") it.entry else library.show(d.entry)) }
    }

    private fun work(block: suspend () -> Unit) {
        _state.update { it.copy(busy = true, message = null) }
        viewModelScope.launch {
            try {
                withContext(io) { block() }
            } catch (e: Exception) {
                AppLog.line("library: ${e.message}")
                _state.update { it.copy(message = e.message ?: "error") }
            } finally {
                _state.update { it.copy(busy = false) }
            }
        }
    }

    override fun onCleared() {
        // Rule 8: undo lasts until the app closes.
        File(workDir, "undo").deleteRecursively()
    }
}
