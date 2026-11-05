package net.tisma.app.ui

import android.content.res.Configuration
import androidx.activity.compose.BackHandler
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import net.tisma.app.R
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.data.AppSettings
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.engine.SliceViewModel

/**
 * The sections (owner's decision, 05-11-2026, over mobile_app.md, rule 5):
 * Library, Plate, Engineering, Printer and More. The app starts in «Mi
 * biblioteca»; Engineering (FEA, CFD, CAD, parts) has its own section; the
 * Print settings open from the plate and the Material ones from the printer.
 */
enum class Section(@StringRes val label: Int, @DrawableRes val icon: Int) {
    LIBRARY(R.string.section_library, R.drawable.tisma_folder),
    PLATE(R.string.section_plate, R.drawable.tisma_plate),
    ENGINEERING(R.string.section_engineering, R.drawable.tisma_engineering),
    PRINTER(R.string.section_printer, R.drawable.tisma_printer),
    MORE(R.string.section_more, R.drawable.tisma_more),
}

/** A screen on top of a section. */
sealed interface Screen {
    data object Account : Screen
    data object Register : Screen
    data object Forgot : Screen
    data object Jobs : Screen
    data class JobDetail(val id: String) : Screen
    data object NewJob : Screen
    /** Report a problem; [crashLog] when offered after an unexpected close (problem_reports.md, rule 10). */
    data class Report(val crashLog: String? = null) : Screen
    data object MyReports : Screen
    data object Preferences : Screen
    data class Preview(val path: String) : Screen
    data object ProfilePicker : Screen
    data object Credits : Screen
    data class AddPrinter(val editId: String?, val found: net.tisma.printers.FoundPrinter? = null) : Screen
    data class PrinterDetail(val id: String) : Screen
    data class SettingsSection(val tab: String, val section: String) : Screen
    data class CustomPicker(val tab: String) : Screen
    data object PrinterSettings : Screen
    data object Messages : Screen
    data object PackWizard : Screen
    data object Help : Screen
    data object Parts : Screen
    data object Cad : Screen
    data object Calibrate : Screen
    /** The Print or Material settings tab, opened from the plate. */
    data class SettingsTab(val tab: String) : Screen
}

/** Where the person is: a section and the screens opened on it. */
class Navigator(start: Section = Section.LIBRARY) {
    var section by mutableStateOf(start)
        private set
    val stack = mutableStateListOf<Screen>()

    /** The notices shown, newest first, to read them again (accessibility_touch.md, rule 8). */
    val notices = mutableStateListOf<String>()

    fun select(s: Section) {
        section = s
        stack.clear()
    }

    fun open(screen: Screen) {
        stack.add(screen)
    }

    /** Puts [screen] in place of the one on top. */
    fun replace(screen: Screen) {
        if (stack.isNotEmpty()) stack.removeAt(stack.lastIndex)
        stack.add(screen)
    }

    fun back(): Boolean {
        if (stack.isEmpty()) return false
        stack.removeAt(stack.lastIndex)
        return true
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun TismaApp(
    cloud: CloudViewModel,
    printers: PrintersViewModel,
    slice: SliceViewModel,
    settings: AppSettings,
    nav: Navigator = remember { Navigator() },
    updates: net.tisma.app.updates.UpdatesViewModel? = null,
    library: net.tisma.app.engine.LibraryViewModel? = null,
) {
    val state by cloud.state.collectAsStateWithLifecycle()
    val printerState by printers.state.collectAsStateWithLifecycle()
    val sliceState by slice.state.collectAsStateWithLifecycle()
    val snackbar = remember { SnackbarHostState() }
    val context = ctx()
    LaunchedEffect(Unit) { cloud.refreshPublic() }
    LaunchedEffect(Unit) { updates?.checkOnOpen() }
    LaunchedEffect(sliceState.packAvailable) { if (sliceState.packAvailable) slice.checkPackOnOpen() }
    // MA1: the first start with an engine and no printer opens the profile pack's wizard, once.
    LaunchedEffect(sliceState.packAvailable) {
        if (sliceState.packAvailable && !settings.packWizardDone && sliceState.profile == null && nav.stack.isEmpty()) {
            nav.select(Section.PLATE)
            nav.open(Screen.PackWizard)
        }
    }
    // Notices: each is taken off the model and shown in its own coroutine, so clearing it
    // (which changes the state) never cancels the message on screen (QA AND1).
    LaunchedEffect(Unit) {
        val show: (String) -> Unit = { text ->
            android.util.Log.i("Tisma", text)
            // accessibility_touch.md, rule 8: a notice stays until it is closed (a newer one takes its place),
            // and every notice can be read again in More › Messages.
            nav.notices.add(0, text)
            if (nav.notices.size > 50) nav.notices.removeAt(nav.notices.lastIndex)
            snackbar.currentSnackbarData?.dismiss()
            launch { snackbar.showSnackbar(text, withDismissAction = true, duration = androidx.compose.material3.SnackbarDuration.Indefinite) }
        }
        launch {
            cloud.state.map { it.notice }.distinctUntilChanged().filterNotNull().collect { n ->
                cloud.dismissNotice()
                if (n == net.tisma.app.cloud.Notice.SessionExpired) {
                    // Rule 2: the interface asks to sign in again.
                    nav.select(Section.MORE)
                    nav.open(Screen.Account)
                }
                show(context.noticeText(n))
            }
        }
        launch {
            printers.state.map { it.notice }.distinctUntilChanged().filterNotNull().collect { n ->
                printers.dismissNotice()
                show(context.printerNoticeText(n))
            }
        }
        launch {
            slice.state.map { it.gcodeToPreview }.distinctUntilChanged().filterNotNull().collect { path ->
                slice.gcodeOpened()
                nav.select(Section.PLATE)
                nav.open(Screen.Preview(path))
            }
        }
        launch {
            slice.state.map { it.notice }.distinctUntilChanged().filterNotNull().collect { n ->
                slice.dismissNotice()
                show(context.sliceNoticeText(n))
            }
        }
    }
    // local_library.md, rule 10: a shared or opened .tismalib is imported into «Mi biblioteca».
    LaunchedEffect(sliceState.libraryFile) {
        sliceState.libraryFile?.let { f ->
            library?.import(f)
            nav.select(Section.LIBRARY)
            slice.libraryFileTaken()
        }
    }
    BackHandler(enabled = nav.stack.isNotEmpty()) { nav.back() }

    val landscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
    val top = nav.stack.lastOrNull()
    Row(Modifier.fillMaxSize()) {
        if (landscape) {
            NavigationRail {
                Section.entries.forEach { s ->
                    NavigationRailItem(
                        selected = nav.section == s,
                        onClick = { nav.select(s) },
                        icon = { Icon(painterResource(s.icon), null) },
                        label = { NavLabel(s.label) },
                    )
                }
            }
        }
        Scaffold(
            modifier = Modifier.weight(1f),
            topBar = {
                TopAppBar(
                    title = { Text(top?.let { stringResource(titleOf(it)) } ?: stringResource(nav.section.label)) },
                    navigationIcon = {
                        if (top != null) {
                            IconButton(onClick = { nav.back() }, modifier = Modifier.testTag("back")) {
                                Icon(painterResource(R.drawable.ic_back), stringResource(R.string.back),
                                    tint = MaterialTheme.colorScheme.onSurface)
                            }
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.background),
                )
            },
            bottomBar = {
                if (!landscape) {
                    NavigationBar(containerColor = MaterialTheme.colorScheme.surfaceContainer) {
                        Section.entries.forEach { s ->
                            NavigationBarItem(
                                selected = nav.section == s,
                                onClick = { nav.select(s) },
                                icon = { Icon(painterResource(s.icon), null) },
                                label = { NavLabel(s.label) },
                                colors = NavigationBarItemDefaults.colors(
                                    selectedIconColor = Tisma.colors.accentText,
                                    selectedTextColor = Tisma.colors.accentText,
                                ),
                                modifier = Modifier.testTag("section_" + s.name.lowercase()),
                            )
                        }
                    }
                }
            },
            snackbarHost = { SnackbarHost(snackbar) },
            containerColor = MaterialTheme.colorScheme.background,
        ) { padding ->
            Column(Modifier.padding(padding).fillMaxSize()) {
            // profile_pack.md, rule 19: «New profiles: update», a small line that covers nothing.
            sliceState.packNewer?.let { v ->
                androidx.compose.material3.Surface(color = MaterialTheme.colorScheme.secondaryContainer,
                    modifier = Modifier.fillMaxWidth().testTag("pack_newer")) {
                    Row(Modifier.padding(horizontal = 16.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text(if (sliceState.packBusy) stringResource(R.string.pack_updating, sliceState.packProgress)
                            else stringResource(R.string.pack_newer, v), style = MaterialTheme.typography.bodySmall, modifier = Modifier.weight(1f))
                        if (!sliceState.packBusy) androidx.compose.material3.TextButton(onClick = { slice.updatePack() },
                            modifier = Modifier.testTag("pack_update")) { Text(stringResource(R.string.pack_update)) }
                    }
                }
            }
            updates?.let { u ->
                val us by u.state.collectAsStateWithLifecycle()
                UpdateBanner(us, u, busy = sliceState.slicing || sliceState.analyzing || printerState.uploading)
            }
            Box(Modifier.weight(1f).fillMaxSize()) {
                when (top) {
                    null -> when (nav.section) {
                        Section.PRINTER -> PrintersScreen(printerState, printers, nav, settings.searchNetwork) {
                            PrinterSettingsCard(sliceState, nav)
                            // The Material settings live with the printer (owner's decision, 05-11-2026).
                            androidx.compose.material3.OutlinedButton(onClick = { nav.open(Screen.SettingsTab("material")) },
                                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("printer_material_settings")) {
                                Icon(painterResource(R.drawable.tisma_filament), null)
                                Spacer(Modifier.width(8.dp))
                                Text(stringResource(R.string.section_material))
                            }
                        }
                        Section.PLATE -> PlateScreen(sliceState, slice, printerState, printers, state.signedIn, nav)
                        Section.ENGINEERING -> PlateScreen(sliceState, slice, printerState, printers, state.signedIn, nav,
                            engineering = true)
                        Section.LIBRARY -> if (library != null && sliceState.libraryAvailable) LibraryScreen(library, slice, nav)
                            else LibraryUnavailable(nav)
                        else -> SectionHome(nav.section, state.signedIn, state.email, nav, updates?.let { u -> {
                            // Rule 1: «Buscar actualizaciones» in More.
                            MoreItem(R.string.updates_check, null, "more_updates") { u.check() }
                        } })
                    }
                    Screen.Account -> AccountScreen(state, cloud, nav)
                    Screen.Register -> RegisterScreen(state, cloud, nav)
                    Screen.Forgot -> ForgotScreen(state, cloud, nav)
                    Screen.Jobs -> JobsScreen(state, cloud, nav)
                    is Screen.JobDetail -> JobDetailScreen(top.id, state, cloud, nav)
                    Screen.NewJob -> NewJobScreen(state, cloud, nav)
                    is Screen.Report -> ReportScreen(state, cloud, nav, top.crashLog)
                    Screen.MyReports -> MyReportsScreen(state, cloud)
                    Screen.Preferences -> PreferencesScreen(state, cloud, settings)
                    is Screen.AddPrinter -> AddPrinterScreen(top.editId, printerState, printers, nav, top.found)
                    is Screen.PrinterDetail -> PrinterDetailScreen(top.id, printerState, printers, nav)
                    is Screen.Preview -> PreviewScreen(top.path, settings.colorVision)
                    Screen.ProfilePicker -> ProfilePickerScreen(sliceState, slice, nav)
                    Screen.Credits -> CreditsScreen()
                    is Screen.SettingsSection -> SettingsSectionScreen(top.tab, top.section, sliceState, slice)
                    is Screen.CustomPicker -> CustomPickerScreen(top.tab, sliceState, slice)
                    Screen.Messages -> MessagesScreen(nav.notices)
                    Screen.Parts -> PartsScreen(slice, nav)
                    Screen.Cad -> CadScreen(sliceState, slice, nav)
                    Screen.Calibrate -> CalibrateScreen(slice, nav)
                    is Screen.SettingsTab -> SettingsTabScreen(top.tab, sliceState, slice, nav,
                        header = if (top.tab == "material") ({ FilamentCard(sliceState, slice, nav) }) else null)
                    Screen.Help -> HelpScreen(slice.knowledge(), { k, v -> slice.setValue(k, v) }) { key ->
                        sliceState.layout?.rows?.get(key)?.label ?: key
                    }
                    Screen.PackWizard -> PackWizardScreen(sliceState, slice, nav) { settings.packWizardDone = true }
                    Screen.PrinterSettings -> PrinterSettingsScreen(sliceState, slice, nav, settings.printerNotes(sliceState.profile?.printer)) {
                        settings.setPrinterNotes(sliceState.profile?.printer, it)
                    }
                }
            }
            }
        }
    }
}

@StringRes
private fun titleOf(s: Screen): Int = when (s) {
    Screen.Account, Screen.Register, Screen.Forgot -> R.string.account_title
    Screen.Jobs, is Screen.JobDetail -> R.string.jobs_title
    Screen.NewJob -> R.string.new_job
    is Screen.Report -> R.string.report_title
    Screen.MyReports -> R.string.more_my_reports
    Screen.Preferences -> R.string.more_preferences
    is Screen.AddPrinter -> R.string.printer_add
    is Screen.PrinterDetail -> R.string.section_printer
    is Screen.Preview -> R.string.preview_title
    Screen.ProfilePicker -> R.string.section_printer
    Screen.Credits -> R.string.more_credits
    is Screen.SettingsSection -> when (s.tab) {
        "material" -> R.string.section_material
        "printer" -> R.string.printer_settings
        else -> R.string.section_print
    }
    Screen.PrinterSettings -> R.string.printer_settings
    Screen.Messages -> R.string.more_messages
    Screen.PackWizard -> R.string.pack_title
    Screen.Help -> R.string.more_help
    Screen.Parts -> R.string.more_parts
    Screen.Cad -> R.string.more_cad
    Screen.Calibrate -> R.string.more_calibrate
    is Screen.SettingsTab -> if (s.tab == "material") R.string.section_material else R.string.section_print
    is Screen.CustomPicker -> R.string.panel_choose
}

@Composable
private fun SectionHome(section: Section, signedIn: Boolean, email: String?, nav: Navigator, updateItem: (@Composable () -> Unit)? = null) {
    when (section) {
        Section.PLATE -> Placeholder(R.string.plate_coming) {
            Button(onClick = { nav.open(if (signedIn) Screen.NewJob else Screen.Account) },
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp).testTag("plate_slice_cloud")) {
                Text(stringResource(R.string.plate_slice_cloud))
            }
        }
        Section.LIBRARY, Section.ENGINEERING -> Unit
        Section.PRINTER -> Placeholder(R.string.printer_coming)
        Section.MORE -> MoreHome(signedIn, email, nav, updateItem)
    }
}

@Composable
private fun Placeholder(@StringRes text: Int, action: (@Composable () -> Unit)? = null) {
    Column(
        Modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text(stringResource(text), style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (action != null) {
            Spacer(Modifier.height(24.dp))
            action()
        }
    }
}

@Composable
private fun MoreHome(signedIn: Boolean, email: String?, nav: Navigator, updateItem: (@Composable () -> Unit)? = null) {
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        MoreItem(R.string.more_cloud, if (signedIn) stringResource(R.string.more_signed_in_as, email.orEmpty())
            else stringResource(R.string.more_signed_out), "more_cloud") { nav.open(Screen.Account) }
        MoreItem(R.string.more_jobs, null, "more_jobs") { nav.open(Screen.Jobs) }
        MoreItem(R.string.more_report, null, "more_report") { nav.open(Screen.Report()) }
        if (signedIn) MoreItem(R.string.more_my_reports, null, "more_my_reports") { nav.open(Screen.MyReports) }
        MoreItem(R.string.more_preferences, null, "more_preferences") { nav.open(Screen.Preferences) }
        MoreItem(R.string.more_calibrate, null, "more_calibrate") { nav.open(Screen.Calibrate) }
        MoreItem(R.string.more_help, null, "more_help") { nav.open(Screen.Help) }
        MoreItem(R.string.more_messages, null, "more_messages") { nav.open(Screen.Messages) }
        updateItem?.invoke()
        MoreItem(R.string.more_credits, null, "more_credits") { nav.open(Screen.Credits) }
    }
}

@Composable
private fun MoreItem(@StringRes title: Int, detail: String?, tag: String, onClick: () -> Unit) {
    Card(onClick = onClick, modifier = Modifier.fillMaxWidth().testTag(tag)) {
        Column(Modifier.padding(PaddingValues(horizontal = 16.dp, vertical = 14.dp))) {
            Text(stringResource(title), style = MaterialTheme.typography.titleMedium)
            if (detail != null) {
                Text(detail, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

/** The notices shown in this session, newest first (accessibility_touch.md, rule 8). */
@Composable
private fun MessagesScreen(notices: List<String>) {
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp).testTag("messages"),
        verticalArrangement = Arrangement.spacedBy(8.dp)) {
        if (notices.isEmpty()) Text(stringResource(R.string.messages_empty), color = MaterialTheme.colorScheme.onSurfaceVariant)
        notices.forEach { Text(it, style = MaterialTheme.typography.bodyMedium) }
    }
}

/**
 * The update notice (app_updates.md, rule 3): a small line at the top that
 * covers nothing, «Version 1.0.1 ready · See what's new», the download's
 * progress (rule 4), and «Install», never while slicing, analyzing or sending
 * a file (rule 6). Android's installer asks the person (rule 11).
 */
@Composable
private fun UpdateBanner(s: net.tisma.app.updates.UpdatesState, u: net.tisma.app.updates.UpdatesViewModel, busy: Boolean) {
    val context = ctx()
    var notes by remember { mutableStateOf(false) }
    val offer = s.offer
    val text = when {
        s.damaged -> stringResource(R.string.updates_damaged)
        offer != null && s.ready != null -> stringResource(R.string.updates_ready, offer.release.version)
        offer != null && s.downloading -> stringResource(R.string.updates_downloading, offer.release.version, s.progress)
        offer != null -> stringResource(R.string.updates_available, offer.release.version)
        s.upToDate -> stringResource(R.string.updates_up_to_date)
        else -> null
    } ?: return
    androidx.compose.material3.Surface(color = MaterialTheme.colorScheme.secondaryContainer, modifier = Modifier.fillMaxWidth().testTag("update_banner")) {
        Row(Modifier.padding(horizontal = 16.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(text, style = MaterialTheme.typography.bodySmall, modifier = Modifier.weight(1f))
            if (offer != null) {
                androidx.compose.material3.TextButton(onClick = { notes = true }) { Text(stringResource(R.string.updates_notes)) }
                when {
                    s.ready != null -> androidx.compose.material3.TextButton(onClick = { installApk(context, s.ready) }, enabled = !busy,
                        modifier = Modifier.testTag("update_install")) { Text(stringResource(R.string.updates_install)) }
                    !s.downloading -> androidx.compose.material3.TextButton(onClick = { u.download() },
                        modifier = Modifier.testTag("update_download")) { Text(stringResource(R.string.updates_download)) }
                }
            }
        }
    }
    if (notes && offer != null) {
        androidx.compose.material3.AlertDialog(onDismissRequest = { notes = false },
            confirmButton = { androidx.compose.material3.TextButton(onClick = { notes = false }) { Text(stringResource(android.R.string.ok)) } },
            title = { Text(stringResource(R.string.updates_available, offer.release.version)) },
            text = { Text(net.tisma.app.updates.Updates.notes(offer.release, if (context.isSpanish()) "es" else "en")) })
    }
}

/** Hands the checked APK to Android's installer, which asks the person (rule 11). */
private fun installApk(context: android.content.Context, apk: java.io.File) {
    val uri = androidx.core.content.FileProvider.getUriForFile(context, context.packageName + ".updates", apk)
    val intent = android.content.Intent(android.content.Intent.ACTION_VIEW)
        .setDataAndType(uri, "application/vnd.android.package-archive")
        .addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION or android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
    runCatching { context.startActivity(intent) }
}

/**
 * A section's name in the bottom bar or the rail: one line, its size growing
 * with the phone's text size up to 1.15 times (accessibility_touch.md, rule 5:
 * nothing cut; at 200 % the names broke in the middle of the word).
 */
@Composable
private fun NavLabel(@StringRes label: Int) {
    val scale = androidx.compose.ui.platform.LocalDensity.current.fontScale
    val size = MaterialTheme.typography.labelMedium.fontSize.value * minOf(scale, 1.15f) / scale
    Text(stringResource(label), maxLines = 1, softWrap = false, fontSize = androidx.compose.ui.unit.TextUnit(size, androidx.compose.ui.unit.TextUnitType.Sp))
}

/** «Mi biblioteca» needs an engine with its `library` commands; until then, the plate. */
@Composable
private fun LibraryUnavailable(nav: Navigator) {
    Placeholder(R.string.library_unavailable) {
        Button(onClick = { nav.select(Section.PLATE) }, modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp).testTag("library_to_plate")) {
            Text(stringResource(R.string.library_to_plate))
        }
    }
}
