package net.tisma.app.updates

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
import net.tisma.app.data.AppSettings
import java.io.File

data class UpdatesState(
    /** False when the app came from a store (the store updates it) or has no key to check with. */
    val enabled: Boolean,
    val checking: Boolean = false,
    val offer: Updates.Offer? = null,
    /** AU11: a newer release that needs a newer Android. */
    val needsNewerSystem: Updates.Release? = null,
    val downloading: Boolean = false,
    val progress: Int = 0,
    /** The checked APK, ready for Android's installer. */
    val ready: File? = null,
    val damaged: Boolean = false,
    val upToDate: Boolean = false,
    val error: String? = null,
)

/** The updates (app_updates.md): a check on opening at most once a day, by hand from More, and the download. */
class UpdatesViewModel(
    private val updater: AppUpdater?,
    private val settings: AppSettings,
    private val installed: String,
    private val sdk: Int,
    private val io: CoroutineDispatcher = Dispatchers.IO,
    private val now: () -> Long = System::currentTimeMillis,
) : ViewModel() {
    private val _state = MutableStateFlow(UpdatesState(enabled = updater != null))
    val state: StateFlow<UpdatesState> = _state.asStateFlow()

    /** Rule 1: on opening, if a day went by since the last check. */
    fun checkOnOpen() {
        if (updater == null || !settings.updatesCheck) return
        if (now() - settings.updatesLastCheck < 24L * 3600 * 1000) return
        check()
    }

    fun check() {
        val u = updater ?: return
        if (_state.value.checking || _state.value.downloading) return
        _state.update { it.copy(checking = true, error = null, upToDate = false) }
        viewModelScope.launch {
            val r = withContext(io) { runCatching { u.releases() } }
            // Without network nothing is said, and it tries again the next day (rule 1).
            r.onFailure { AppLog.line("updates: ${it.message}") }
            if (r.isSuccess) settings.updatesLastCheck = now()
            val choice = r.getOrNull()?.let { Updates.choose(it, settings.updatesChannel, sdk, installed) }
            _state.update {
                it.copy(checking = false, offer = choice?.offer, needsNewerSystem = choice?.needsNewerSystem,
                    upToDate = r.isSuccess && choice?.offer == null, error = r.exceptionOrNull()?.message)
            }
            if (choice?.offer != null && settings.updatesAutoDownload) download()
        }
    }

    /** Rule 4: in the background; a cut download goes on next time. */
    fun download() {
        val u = updater ?: return
        val offer = _state.value.offer ?: return
        if (_state.value.downloading) return
        _state.update { it.copy(downloading = true, progress = 0, damaged = false, error = null) }
        viewModelScope.launch {
            val r = withContext(io) { runCatching { u.download(offer, { p -> _state.update { it.copy(progress = p) } }) { false } } }
            r.onFailure { AppLog.line("updates: download ${it.message}") }
            _state.update {
                it.copy(downloading = false, ready = r.getOrNull(),
                    damaged = r.exceptionOrNull()?.message == "damaged",
                    error = r.exceptionOrNull()?.message?.takeIf { m -> m != "damaged" })
            }
        }
    }
}
