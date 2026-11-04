package net.tisma.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import net.tisma.app.cloud.CloudViewModel
import net.tisma.app.ui.TismaApp
import net.tisma.app.ui.TismaTheme

class MainActivity : ComponentActivity() {
    private val cloud: CloudViewModel by viewModels {
        val app = application as TismaApplication
        object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                CloudViewModel(app.settings, app.secrets, app.deviceName) as T
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        val app = application as TismaApplication
        setContent {
            TismaTheme {
                TismaApp(cloud, app.settings)
            }
        }
    }
}
