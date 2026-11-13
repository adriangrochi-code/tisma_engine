package net.tisma.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.printers.PrinterStore
import net.tisma.app.printers.PrintWatchService
import net.tisma.app.engine.Catalog
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SliceViewModel
import net.tisma.app.printers.PrintersViewModel
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme

class MainActivity : ComponentActivity() {
    private val cloud: CloudViewModel by viewModels {
        val app = application as TismaApplication
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                CloudViewModel(app.settings, app.secrets, app.deviceName, cacheDir = app.cacheDir) as T
        }
    }

    private val printers: PrintersViewModel by viewModels {
        val app = application as TismaApplication
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                PrintersViewModel(PrinterStore(app, app.secrets), onActivePrint = { PrintWatchService.start(app) }) as T
        }
    }

    private val updates: net.tisma.app.updates.UpdatesViewModel by viewModels {
        val app = application as TismaApplication
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                val key = net.tisma.app.updates.AppUpdater.hexKey(BuildConfig.UPDATES_KEY)
                // From a store, the store updates the app (app_updates.md, rule 11).
                val updater = if (key == null || fromStore()) null else net.tisma.app.updates.AppUpdater(
                    net.tisma.cloud.UrlConnectionHttp, key,
                    listOf(net.tisma.app.updates.AppUpdater.GITHUB_LIST, net.tisma.app.updates.AppUpdater.cloudList(app.settings.server)),
                    java.io.File(app.cacheDir, "updates"))
                return net.tisma.app.updates.UpdatesViewModel(updater, app.settings, BuildConfig.VERSION_NAME,
                    android.os.Build.VERSION.SDK_INT) as T
            }
        }
    }

    private fun fromStore(): Boolean {
        val installer = runCatching {
            if (android.os.Build.VERSION.SDK_INT >= 30) packageManager.getInstallSourceInfo(packageName).installingPackageName
            else @Suppress("DEPRECATION") packageManager.getInstallerPackageName(packageName)
        }.getOrNull()
        return installer != null && installer != "com.google.android.packageinstaller" && installer != "com.android.packageinstaller"
            && installer != "com.android.shell"
    }

    private val slice: SliceViewModel by viewModels {
        val app = application as TismaApplication
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                val work = java.io.File(app.cacheDir, "engine")
                val exe = java.io.File(app.applicationInfo.nativeLibraryDir, EngineRunner.FILE_NAME)
                val catalog = {
                    Catalog.parse(app.assets.open("catalog/printers.json").bufferedReader().readText(),
                        app.assets.open("catalog/filaments.json").bufferedReader().readText())
                }
                val free = {
                    val am = app.getSystemService(android.app.ActivityManager::class.java)
                    android.app.ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }.availMem
                }
                val layout = {
                    val lang = if (java.util.Locale.getDefault().language == "es") "es" else "en"
                    net.tisma.app.engine.SettingsLayout.parse(app.assets.open("settings/layout_$lang.json").bufferedReader().readText())
                }
                // The profile pack lives with the app's files, apart from the cache (profile_pack.md, rule 8).
                val packDir = java.io.File(app.filesDir, "profiles")
                val runner = EngineRunner(exe, work, packDir)
                return SliceViewModel(runner, work, loadCatalog = catalog, saved = app.settings,
                    freeMemory = free, loadLayout = layout,
                    pack = if (runner.available) net.tisma.app.engine.ProfilePack(exe, packDir) else null,
                    library = if (runner.available) libraryOf(app) else null) as T
            }
        }
    }

    /** «Mi biblioteca» in the app's own storage (local_library.md, rule 11). */
    private fun libraryOf(app: TismaApplication) = net.tisma.app.engine.Library(
        java.io.File(app.applicationInfo.nativeLibraryDir, EngineRunner.FILE_NAME), java.io.File(app.filesDir, "library"))

    private val library: net.tisma.app.engine.LibraryViewModel by viewModels {
        val app = application as TismaApplication
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                net.tisma.app.engine.LibraryViewModel(libraryOf(app), java.io.File(app.cacheDir, "library")) as T
        }
    }

    /** The shared library (p2p_library.md): its block list is checked with Tisma's key (none yet: sharing stays off). */
    private val shared: net.tisma.app.p2p.SharedViewModel by viewModels {
        val app = application as TismaApplication
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T = net.tisma.app.p2p.SharedViewModel(
                java.io.File(app.filesDir, "p2p"), net.tisma.app.updates.AppUpdater.hexKey(BuildConfig.UPDATES_KEY) ?: ByteArray(0),
                server = object : net.tisma.app.p2p.LibraryServer {
                    // Rule 3 bis: the library's own files need no session; «Denunciar» goes with the person's.
                    override fun file(name: String) = net.tisma.cloud.CloudClient(app.settings.server, null).libraryFile(name)
                    override fun report(model: String?, sha256: String?, reason: String) =
                        net.tisma.cloud.CloudClient(app.settings.server, app.secrets.get(net.tisma.app.cloud.CloudViewModel.TOKEN))
                            .libraryReport(model, sha256, reason)
                    override val origin: String get() = app.settings.server
                },
                saved = app.settings) as T
        }
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        handleShared(intent)
    }

    /** A model shared from another app or opened with Tisma goes to the plate (MA5). */
    private fun handleShared(intent: android.content.Intent?) {
        val uri: android.net.Uri = sharedUri(intent) ?: return
        val name = runCatching {
            contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) c.getString(0) else null
            }
        }.getOrNull() ?: uri.lastPathSegment?.substringAfterLast('/') ?: "model.stl"
        slice.open(name) { contentResolver.openInputStream(uri) }
        nav.select(net.tisma.app.ui.Section.PLATE)
    }

    /** The section and screens kept through a turn of the phone (MA4: the screen rearranges, nothing else changes). */
    class NavHolder : ViewModel() { val nav = net.tisma.app.ui.Navigator() }
    private val navHolder: NavHolder by viewModels()
    private val nav get() = navHolder.nav
    internal val navigator get() = nav

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        val app = application as TismaApplication
        // Only the first time: a turn of the phone recreates the activity with the same intent, which would open the
        // shared file again over what the person did since.
        if (savedInstanceState == null) handleShared(intent)
        // After an unexpected close: offer to report it, once (problem_reports.md, rule 10).
        if (savedInstanceState == null) app.crash.take()?.let { log ->
            nav.select(net.tisma.app.ui.Section.MORE)
            nav.open(net.tisma.app.ui.Screen.Report(log))
        }
        setContent {
            TismaTheme {
                TismaApp(cloud, printers, slice, app.settings, nav, updates, library, shared)
            }
        }
    }
}

/** The model's address in a «Share» or «Open with» intent, if there is one. */
fun sharedUri(intent: android.content.Intent?): android.net.Uri? = when (intent?.action) {
    android.content.Intent.ACTION_SEND ->
        if (android.os.Build.VERSION.SDK_INT >= 33) intent.getParcelableExtra(android.content.Intent.EXTRA_STREAM, android.net.Uri::class.java)
        else @Suppress("DEPRECATION") intent.getParcelableExtra(android.content.Intent.EXTRA_STREAM)
    android.content.Intent.ACTION_VIEW -> intent.data
    else -> null
}
