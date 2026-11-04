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
                return SliceViewModel(EngineRunner(exe, work), work, loadCatalog = catalog, saved = app.settings,
                    freeMemory = free) as T
            }
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

    private val nav = net.tisma.app.ui.Navigator()

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        val app = application as TismaApplication
        handleShared(intent)
        setContent {
            TismaTheme {
                TismaApp(cloud, printers, slice, app.settings, nav)
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
