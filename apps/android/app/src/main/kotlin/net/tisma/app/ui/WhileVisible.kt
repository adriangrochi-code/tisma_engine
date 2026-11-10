package net.tisma.app.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.CoroutineScope

/**
 * Runs [block] while the screen is on and visible, and stops it when the app goes to the background (it starts again
 * on return). A plain LaunchedEffect keeps running in the background: polling printers or the camera there costs
 * battery and network for nothing.
 */
@Composable
fun WhileVisible(vararg keys: Any?, block: suspend CoroutineScope.() -> Unit) {
    val owner = LocalLifecycleOwner.current
    LaunchedEffect(owner, *keys) { owner.repeatOnLifecycle(Lifecycle.State.STARTED, block) }
}
