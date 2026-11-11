package net.tisma.app.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import net.tisma.app.R
import net.tisma.app.cloud.CloudState
import net.tisma.app.cloud.CloudViewModel
import net.tisma.cloud.Plan

/** The account panel (cloud.md, rules 2 to 5a): the login, or the account, and the public plans. */
@Composable
fun AccountScreen(state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    // Asked again each time the panel opens (rule 5a).
    LaunchedEffect(state.signedIn) { cloud.refreshAccount() }
    ScreenColumn {
        if (state.signedIn) AccountCard(state, cloud) else LoginCard(state, cloud, nav)
        if (state.signedIn) SessionsCard(state, cloud)
        PlansCard(state, cloud)
    }
}

@Composable
private fun LoginCard(state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    var email by rememberSaveable { mutableStateOf("") }
    var password by rememberSaveable { mutableStateOf("") }
    TismaCard {
        Text(state.server, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        EmailField(email, { email = it })
        PasswordField(password, { password = it }, R.string.password, "password")
        Button(
            onClick = {
                cloud.signIn(email, password)
                // The password is not kept once sent.
                password = ""
            },
            enabled = !state.busy && email.isNotBlank() && password.isNotEmpty(),
            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("sign_in"),
        ) { Text(stringResource(R.string.sign_in)) }
        Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
            AccentTextButton(onClick = { nav.open(Screen.Register) }) { Text(stringResource(R.string.create_account)) }
            AccentTextButton(onClick = { nav.open(Screen.Forgot) }) { Text(stringResource(R.string.forgot_password)) }
        }
        if (state.busy) LinearProgressIndicator(Modifier.fillMaxWidth())
    }
}

@Composable
private fun AccountCard(state: CloudState, cloud: CloudViewModel) {
    val context = ctx()
    val a = state.account
    TismaCard(Modifier.testTag("account")) {
        Text(state.email.orEmpty(), style = MaterialTheme.typography.titleMedium)
        if (a == null) {
            LinearProgressIndicator(Modifier.fillMaxWidth())
        } else {
            Labeled(R.string.plan, a.plan)
            Labeled(R.string.hours_month,
                if (a.unlimited) stringResource(R.string.hours_unlimited, hours(a.hoursUsed))
                else stringResource(R.string.hours_value, hours(a.hoursUsed), hours(a.hoursIncluded), hours(a.hoursLeft)),
                Modifier.testTag("hours"))
            if (a.hoursIncluded > 0 && !a.unlimited) {
                LinearProgressIndicator(
                    progress = { (a.hoursUsed / a.hoursIncluded).toFloat().coerceIn(0f, 1f) },
                    modifier = Modifier.fillMaxWidth(),
                )
            }
            if (a.paidUntil.isNotBlank()) Text(stringResource(R.string.paid_until, shortDate(a.paidUntil).take(10)),
                style = MaterialTheme.typography.bodyMedium)
            if (a.maxConcurrent > 0) Text(stringResource(R.string.jobs_running, a.activeJobs, a.maxConcurrent),
                style = MaterialTheme.typography.bodyMedium)
        }
        // cloud.md 5b: the subscription is bought and cancelled in the store; the app opens its page and holds no
        // payment data. The web page only when the server turned web billing on (off by default).
        OutlinedButton(onClick = { context.openStoreSubscriptions() }, modifier = Modifier.fillMaxWidth().testTag("manage_plan")) {
            Text(stringResource(R.string.manage_plan))
        }
        if (state.billingUrl.isNotBlank()) AccentTextButton(onClick = { context.openAccountPage(state) },
            modifier = Modifier.testTag("manage_plan_web")) { Text(stringResource(R.string.manage_plan_web)) }
        Text(stringResource(R.string.manage_plan_hint), style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        AccentTextButton(onClick = { cloud.signOut() }, modifier = Modifier.testTag("sign_out")) {
            Text(stringResource(R.string.sign_out))
        }
    }
}

/** Where the account is signed in, with this phone marked; the others can be closed. */
@Composable
private fun SessionsCard(state: CloudState, cloud: CloudViewModel) {
    LaunchedEffect(state.email) { cloud.loadSessions() }
    if (state.sessions.isEmpty()) return
    TismaCard(Modifier.testTag("sessions")) {
        Text(stringResource(R.string.sessions_title), style = MaterialTheme.typography.titleMedium)
        state.sessions.forEach { s ->
            Row(verticalAlignment = androidx.compose.ui.Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(s.device.ifBlank { "—" } + if (s.current) " · " + stringResource(R.string.sessions_this) else "",
                        style = MaterialTheme.typography.bodyLarge)
                    Text(shortDate(s.lastUsed.ifBlank { s.createdAt }), style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                if (!s.current) AccentTextButton(onClick = { cloud.closeSession(s.id) }, enabled = !state.busy) {
                    Text(stringResource(R.string.sessions_close))
                }
            }
        }
        if (state.sessions.count { !it.current } > 0) {
            OutlinedButton(onClick = { cloud.closeOtherSessions() }, enabled = !state.busy,
                modifier = Modifier.fillMaxWidth().testTag("close_others")) { Text(stringResource(R.string.sessions_close_others)) }
        }
    }
}

/** The public plans as the server sends them, with the current one marked (rule 5a, CC13). */
@Composable
private fun PlansCard(state: CloudState, cloud: CloudViewModel? = null) {
    val context = ctx()
    Text(stringResource(R.string.plans_title), style = MaterialTheme.typography.titleMedium,
        modifier = Modifier.padding(top = 8.dp))
    if (state.plansLoaded && state.plans.isEmpty()) {
        Text(stringResource(R.string.no_plans), color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
    val current = state.account?.plan
    // cloud.md 5c: a plan is offered for purchase only with a product in this phone's store.
    val store = cloud?.billing
    state.plans.forEach { p ->
        PlanRow(context, p, p.name == current,
            buy = if (store != null && state.signedIn && p.name != current && p.storeProducts[store.store] != null) ({ cloud.buy(p) }) else null)
    }
}

@Composable
private fun PlanRow(context: Context, p: Plan, current: Boolean, buy: (() -> Unit)? = null) {
    val colors = if (current) CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.primaryContainer)
    else CardDefaults.cardColors()
    Card(colors = colors, modifier = Modifier.fillMaxWidth().testTag("plan_" + p.name)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Row(horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
                Text(p.name, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                Text(if (p.priceCents == 0L) stringResource(R.string.plan_free)
                    else stringResource(R.string.plan_price, price(p.priceCents, p.currency)))
            }
            if (current) Text(stringResource(R.string.plan_current), color = Tisma.colors.accentText,
                style = MaterialTheme.typography.labelLarge)
            Text(stringResource(R.string.plan_line, hours(p.monthlyHours), p.maxConcurrent),
                style = MaterialTheme.typography.bodyMedium)
            if (p.classes.isNotEmpty()) {
                Text(stringResource(R.string.plan_includes, p.classes.joinToString(", ") { context.classText(it) }),
                    style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            buy?.let { OutlinedButton(onClick = it, modifier = Modifier.testTag("plan_buy_" + p.name)) { Text(stringResource(R.string.plan_buy)) } }
        }
    }
}

@Composable
fun RegisterScreen(state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    var email by rememberSaveable { mutableStateOf("") }
    var password by rememberSaveable { mutableStateOf("") }
    var code by rememberSaveable { mutableStateOf("") }
    LaunchedEffect(state.signedIn) { if (state.signedIn) nav.replace(Screen.Account) }
    ScreenColumn {
        TismaCard {
            val verifying = state.verifying
            if (verifying == null) {
                Text(stringResource(R.string.create_account), style = MaterialTheme.typography.titleMedium)
                EmailField(email, { email = it })
                PasswordField(password, { password = it }, R.string.password, "password",
                    supporting = stringResource(R.string.password_rule))
                Button(
                    onClick = { cloud.register(email, password, null) },
                    enabled = !state.busy && email.contains('@') && password.length >= 8,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("register"),
                ) { Text(stringResource(R.string.create_account)) }
            } else {
                Text(stringResource(R.string.verify_title), style = MaterialTheme.typography.titleMedium)
                Text(stringResource(R.string.verify_text, verifying))
                CodeField(code) { code = it }
                Button(
                    onClick = { cloud.verify(code) },
                    enabled = !state.busy && code.length == 6,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp).testTag("verify"),
                ) { Text(stringResource(R.string.verify)) }
                AccentTextButton(onClick = { cloud.resendCode() }) { Text(stringResource(R.string.resend_code)) }
            }
            if (state.busy) LinearProgressIndicator(Modifier.fillMaxWidth())
        }
    }
}

@Composable
fun ForgotScreen(state: CloudState, cloud: CloudViewModel, nav: Navigator) {
    var email by rememberSaveable { mutableStateOf("") }
    var code by rememberSaveable { mutableStateOf("") }
    var password by rememberSaveable { mutableStateOf("") }
    var sent by rememberSaveable { mutableStateOf(false) }
    ScreenColumn {
        TismaCard {
            Text(stringResource(R.string.forgot_password), style = MaterialTheme.typography.titleMedium)
            EmailField(email, { email = it })
            if (!sent) {
                Button(onClick = { cloud.forgotPassword(email) { sent = true } },
                    enabled = !state.busy && email.contains('@'),
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text(stringResource(R.string.send_code)) }
            } else {
                CodeField(code) { code = it }
                PasswordField(password, { password = it }, R.string.new_password, "new_password",
                    supporting = stringResource(R.string.password_rule))
                Button(onClick = { cloud.resetPassword(email, code, password) { nav.back() } },
                    enabled = !state.busy && code.length == 6 && password.length >= 8,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text(stringResource(R.string.change_password)) }
            }
            if (state.busy) LinearProgressIndicator(Modifier.fillMaxWidth())
        }
    }
}

// ---- Pieces shared by the screens ----

@Composable
fun ScreenColumn(content: @Composable ColumnScope.() -> Unit) {
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
        content = content,
    )
}

@Composable
fun TismaCard(modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp), content = content)
    }
}

@Composable
fun Labeled(label: Int, value: String, modifier: Modifier = Modifier) {
    Column(modifier) {
        Text(stringResource(label), style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, style = MaterialTheme.typography.bodyLarge)
    }
}

@Composable
private fun EmailField(value: String, onChange: (String) -> Unit) {
    OutlinedTextField(value, onChange, label = { Text(stringResource(R.string.email)) }, singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Email),
        modifier = Modifier.fillMaxWidth().testTag("email"))
}

@Composable
private fun PasswordField(value: String, onChange: (String) -> Unit, label: Int, tag: String, supporting: String? = null) {
    OutlinedTextField(value, onChange, label = { Text(stringResource(label)) }, singleLine = true,
        visualTransformation = PasswordVisualTransformation(),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
        supportingText = supporting?.let { { Text(it) } },
        modifier = Modifier.fillMaxWidth().testTag(tag))
}

@Composable
private fun CodeField(value: String, onChange: (String) -> Unit) {
    OutlinedTextField(value, { v -> onChange(v.filter(Char::isDigit).take(6)) },
        label = { Text(stringResource(R.string.code)) }, singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
        modifier = Modifier.fillMaxWidth().testTag("code"))
}

/** cloud.md 5b: Google Play's page of this app's subscriptions, where a plan is changed or cancelled. */
fun Context.openStoreSubscriptions() {
    val url = "https://play.google.com/store/account/subscriptions?package=" + packageName
    runCatching { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
}

/** Opens the account's page in the browser (rule 5); only http(s) addresses. */
fun Context.openAccountPage(state: CloudState) {
    val url = state.billingUrl.ifBlank { state.server }
    if (!url.startsWith("https://") && !url.startsWith("http://")) return
    runCatching { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
}
