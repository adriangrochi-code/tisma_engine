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
import androidx.compose.foundation.layout.padding
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

/** The five sections of mobile_app.md, rule 5. */
enum class Section(@StringRes val label: Int, @DrawableRes val icon: Int) {
    PLATE(R.string.section_plate, R.drawable.tisma_plate),
    PRINT(R.string.section_print, R.drawable.tisma_print),
    MATERIAL(R.string.section_material, R.drawable.tisma_filament),
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
    data object Report : Screen
    data object MyReports : Screen
    data object Preferences : Screen
    data class Preview(val path: String) : Screen
    data object ProfilePicker : Screen
    data class AddPrinter(val editId: String?, val found: net.tisma.printers.FoundPrinter? = null) : Screen
    data class PrinterDetail(val id: String) : Screen
}

/** Where the person is: a section and the screens opened on it. */
class Navigator {
    var section by mutableStateOf(Section.PLATE)
        private set
    val stack = mutableStateListOf<Screen>()

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
) {
    val state by cloud.state.collectAsStateWithLifecycle()
    val printerState by printers.state.collectAsStateWithLifecycle()
    val sliceState by slice.state.collectAsStateWithLifecycle()
    val snackbar = remember { SnackbarHostState() }
    val context = ctx()
    LaunchedEffect(Unit) { cloud.refreshPublic() }
    // Notices: each is taken off the model and shown in its own coroutine, so clearing it
    // (which changes the state) never cancels the message on screen (QA AND1).
    LaunchedEffect(Unit) {
        val show: (String) -> Unit = { text ->
            android.util.Log.i("Tisma", text)
            launch { snackbar.showSnackbar(text) }
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
            slice.state.map { it.notice }.distinctUntilChanged().filterNotNull().collect { n ->
                slice.dismissNotice()
                show(context.sliceNoticeText(n))
            }
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
                        label = { Text(stringResource(s.label)) },
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
                                label = { Text(stringResource(s.label)) },
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
            Box(Modifier.padding(padding).fillMaxSize()) {
                when (top) {
                    null -> when (nav.section) {
                        Section.PRINTER -> PrintersScreen(printerState, printers, nav, settings.searchNetwork)
                        Section.PLATE -> PlateScreen(sliceState, slice, printerState, printers, state.signedIn, nav)
                        else -> SectionHome(nav.section, state.signedIn, state.email, nav)
                    }
                    Screen.Account -> AccountScreen(state, cloud, nav)
                    Screen.Register -> RegisterScreen(state, cloud, nav)
                    Screen.Forgot -> ForgotScreen(state, cloud, nav)
                    Screen.Jobs -> JobsScreen(state, cloud, nav)
                    is Screen.JobDetail -> JobDetailScreen(top.id, state, cloud, nav)
                    Screen.NewJob -> NewJobScreen(state, cloud, nav)
                    Screen.Report -> ReportScreen(state, cloud, nav)
                    Screen.MyReports -> MyReportsScreen(state, cloud)
                    Screen.Preferences -> PreferencesScreen(state, cloud, settings)
                    is Screen.AddPrinter -> AddPrinterScreen(top.editId, printerState, printers, nav, top.found)
                    is Screen.PrinterDetail -> PrinterDetailScreen(top.id, printerState, printers, nav)
                    is Screen.Preview -> PreviewScreen(top.path)
                    Screen.ProfilePicker -> ProfilePickerScreen(sliceState, slice, nav)
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
    Screen.Report -> R.string.report_title
    Screen.MyReports -> R.string.more_my_reports
    Screen.Preferences -> R.string.more_preferences
    is Screen.AddPrinter -> R.string.printer_add
    is Screen.PrinterDetail -> R.string.section_printer
    is Screen.Preview -> R.string.preview_title
    Screen.ProfilePicker -> R.string.section_printer
}

@Composable
private fun SectionHome(section: Section, signedIn: Boolean, email: String?, nav: Navigator) {
    when (section) {
        Section.PLATE -> Placeholder(R.string.plate_coming) {
            Button(onClick = { nav.open(if (signedIn) Screen.NewJob else Screen.Account) },
                modifier = Modifier.fillMaxWidth().height(56.dp).testTag("plate_slice_cloud")) {
                Text(stringResource(R.string.plate_slice_cloud))
            }
        }
        Section.PRINT, Section.MATERIAL -> Placeholder(R.string.settings_coming)
        Section.PRINTER -> Placeholder(R.string.printer_coming)
        Section.MORE -> MoreHome(signedIn, email, nav)
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
private fun MoreHome(signedIn: Boolean, email: String?, nav: Navigator) {
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)) {
        MoreItem(R.string.more_cloud, if (signedIn) stringResource(R.string.more_signed_in_as, email.orEmpty())
            else stringResource(R.string.more_signed_out), "more_cloud") { nav.open(Screen.Account) }
        MoreItem(R.string.more_jobs, null, "more_jobs") { nav.open(Screen.Jobs) }
        MoreItem(R.string.more_report, null, "more_report") { nav.open(Screen.Report) }
        if (signedIn) MoreItem(R.string.more_my_reports, null, "more_my_reports") { nav.open(Screen.MyReports) }
        MoreItem(R.string.more_preferences, null, "more_preferences") { nav.open(Screen.Preferences) }
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
