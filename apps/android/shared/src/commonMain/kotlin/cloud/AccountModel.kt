package net.tisma.cloud

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** Where the session's token is kept between runs: the Keychain on iOS (cloud.md, rule 3). */
interface TokenStore {
    fun read(): String?
    fun write(token: String?)
}

/** What the account screens show (cloud.md, rules 2 to 5a and 4a). */
data class AccountState(
    val signedIn: Boolean = false,
    val account: Account? = null,
    val plans: List<Plan> = emptyList(),
    val service: ServiceStatus? = null,
    /** One of the person's requests is running: the buttons that send wait. */
    val busy: Boolean = false,
    /** The last request's failure, worded by the screen from its kind. */
    val problem: CloudException? = null,
)

/**
 * The Tisma Cloud account as a state the screens watch (apps/ios/PLAN.md, step 3): the session, the account, the
 * public plans and the service's state. Requests run on [io]; [state] changes are what the screens draw. Android
 * keeps its own CloudViewModel for now; this is the iPhone's, written to serve both.
 */
class AccountModel(
    server: String,
    private val tokens: TokenStore,
    private val device: String,
    private val io: CoroutineDispatcher = Dispatchers.Default,
    private val clientFor: (String, String?) -> CloudClient = { s, t -> CloudClient(s, t) },
) {
    private val scope = CoroutineScope(SupervisorJob() + io)
    private var client = clientFor(server, tokens.read())
    private val _state = MutableStateFlow(AccountState(signedIn = client.token != null))
    val state: StateFlow<AccountState> = _state.asStateFlow()

    /** The panel opened (rule 5a, 4a): the public plans, the service's state and, signed in, the account. */
    fun refresh(): Job = scope.launch {
        runCatching { client.status() }.onSuccess { st -> _state.update { it.copy(service = st) } }
        runCatching { client.plans() }.onSuccess { p -> _state.update { it.copy(plans = p) } }
        if (client.token != null) request { client.account() }?.let { a -> _state.update { it.copy(account = a) } }
    }

    fun signIn(email: String, password: String): Job = scope.launch {
        val session = request(holds = true) { client.login(email.trim(), password, device) } ?: return@launch
        tokens.write(session.token)
        _state.update { it.copy(signedIn = true) }
        request { client.account() }?.let { a -> _state.update { it.copy(account = a) } }
    }

    /** Closes the session here and on the server; nothing of the account is left on screen. */
    fun signOut(): Job = scope.launch {
        runCatching { client.logout() }
        tokens.write(null)
        client = clientFor(client.serverUrl, null)
        _state.update { AccountState(plans = it.plans, service = it.service) }
    }

    fun problemSeen() = _state.update { it.copy(problem = null) }

    /** Watches [state] with a callback, for Swift (which cannot collect a Flow); call the result to stop. */
    fun watch(onChange: (AccountState) -> Unit): () -> Unit {
        val job = scope.launch { state.collect { onChange(it) } }
        return { job.cancel() }
    }

    fun close() = scope.cancel()

    private suspend fun <T> request(holds: Boolean = false, block: () -> T): T? {
        if (holds) _state.update { it.copy(busy = true) }
        return try {
            withContext(io) { block() }.also { _state.update { it.copy(problem = null) } }
        } catch (e: CloudException) {
            if (e.kind == CloudException.Kind.EXPIRED) {
                // The session ended on the server side (rule 2): sign in again.
                tokens.write(null)
                client = clientFor(client.serverUrl, null)
                _state.update { AccountState(plans = it.plans, service = it.service, problem = e) }
            } else {
                _state.update { it.copy(problem = e) }
            }
            null
        } finally {
            if (holds) _state.update { it.copy(busy = false) }
        }
    }
}
