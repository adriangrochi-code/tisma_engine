package net.tisma.app.ui

import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import net.tisma.app.R
import net.tisma.app.printers.PrintersState
import net.tisma.printers.PrinterState

/** The connection icon's states (settings_panel.md, rule 42), on the phone. */
enum class Connection { NONE, FOUND, CONNECTING, CONNECTED, ERROR }

/**
 * The state shown on the plate's printer card: the printer chosen to send to (or the only one saved). With none saved,
 * whether one was found on the network.
 */
fun connectionOf(p: PrintersState, sendTo: String?): Connection {
    val printer = p.printers.firstOrNull { it.id == sendTo } ?: p.printers.singleOrNull() ?: p.printers.firstOrNull()
        ?: return if (p.found.isNotEmpty()) Connection.FOUND else Connection.NONE
    return when (p.status[printer.id]?.state) {
        null -> Connection.CONNECTING
        PrinterState.OFFLINE, PrinterState.ERROR -> Connection.ERROR
        else -> Connection.CONNECTED
    }
}

/** The icon on the printer card; a tap opens the printers (rule 43: connect, add by hand). */
@Composable
fun ConnectionIcon(c: Connection, onClick: () -> Unit) {
    val text = stringResource(when (c) {
        Connection.NONE -> R.string.connection_none
        Connection.FOUND -> R.string.connection_found
        Connection.CONNECTING -> R.string.connection_connecting
        Connection.CONNECTED -> R.string.connection_connected
        Connection.ERROR -> R.string.connection_error
    })
    val tint = when (c) {
        Connection.NONE -> MaterialTheme.colorScheme.onSurfaceVariant
        Connection.FOUND, Connection.CONNECTING -> MaterialTheme.colorScheme.primary
        Connection.CONNECTED -> Color(0xFF2E7D32)
        Connection.ERROR -> MaterialTheme.colorScheme.error
    }
    IconButton(onClick = onClick, modifier = Modifier.testTag("connection_" + c.name.lowercase()).semantics { contentDescription = text }) {
        Icon(painterResource(R.drawable.tisma_connection), null, tint = tint)
    }
}
