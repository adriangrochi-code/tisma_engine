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
                return SliceViewModel(EngineRunner(exe, work), work) as T
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        val app = application as TismaApplication
        setContent {
            TismaTheme {
                TismaApp(cloud, printers, slice, app.settings)
            }
        }
    }
}
