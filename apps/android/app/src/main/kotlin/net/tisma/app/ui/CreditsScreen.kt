package net.tisma.app.ui

import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import net.tisma.app.R
import net.tisma.app.data.Credits

/** Más › «Credits and sources» (docs/spec/features/credits.md, rule 4). */
@Composable
fun CreditsScreen() {
    val context = ctx()
    val credits = remember {
        runCatching {
            Credits.parse(context.assets.open("credits/credits.json").bufferedReader().readText(),
                context.assets.open("credits/credits_android.json").bufferedReader().readText(), context.isSpanish())
        }.getOrNull()
    }
    var notice by rememberSaveable { mutableStateOf<String?>(null) }
    ScreenColumn {
        if (credits == null) {
            Text(stringResource(R.string.error_cannot_read))
            return@ScreenColumn
        }
        Text(stringResource(R.string.credits_features), style = MaterialTheme.typography.titleMedium)
        credits.features.forEach { f ->
            Card(onClick = { open(context, f.url) }, modifier = Modifier.fillMaxWidth()) {
                Column(Modifier.padding(12.dp)) {
                    Text(f.name, style = MaterialTheme.typography.bodyLarge)
                    Text(stringResource(R.string.credits_based_on, f.source, f.author) + " · " + f.license,
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        }
        Text(stringResource(R.string.credits_libraries), style = MaterialTheme.typography.titleMedium,
            modifier = Modifier.padding(top = 8.dp))
        credits.libraries.forEach { l ->
            Card(onClick = { notice = l.notices.firstOrNull() }, modifier = Modifier.fillMaxWidth().testTag("lib_" + l.name)) {
                Column(Modifier.padding(12.dp)) {
                    Text(l.name + " " + l.version, style = MaterialTheme.typography.bodyLarge)
                    Text(l.license + " · " + l.url, style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        }
    }
    notice?.let { n ->
        val text = remember(n) {
            runCatching { context.assets.open("credits/licenses/$n").bufferedReader().readText() }.getOrDefault(n)
        }
        AlertDialog(
            onDismissRequest = { notice = null },
            confirmButton = { AccentTextButton(onClick = { notice = null }) { Text(stringResource(R.string.ok)) } },
            title = { Text(n) },
            text = {
                Column(Modifier.heightIn(max = 480.dp).verticalScroll(rememberScrollState())) {
                    Text(text, fontFamily = FontFamily.Monospace, style = MaterialTheme.typography.bodySmall)
                }
            },
        )
    }
}

private fun open(context: android.content.Context, url: String) {
    if (!url.startsWith("https://") && !url.startsWith("http://")) return
    runCatching { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
}
