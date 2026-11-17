package net.tisma.printers

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.doubleOrNull
import net.tisma.cloud.Http
import net.tisma.io.ByteInput
import net.tisma.io.Lock
import net.tisma.io.nowMs
import net.tisma.io.sleepMs
import net.tisma.io.startDaemon

/**
 * Creality printers with their stock firmware (printer_connections.md, «Creality de fábrica»; the owner's decision of
 * 17-11-2026: only the state, sending goes through Moonraker). The same behaviour as the desktop's (dev 808e784):
 *
 * - If Moonraker answers on 7125 (rooted, or a firmware that has it), that connection is used for everything (CR1-1).
 * - Otherwise the state comes from the WebSocket the printer pushes on `ws://host:9999/`: `printJobTime` and
 *   `printLeftTime` (seconds) give the times and the progress; `model`, or «Creality (board <modelVersion>)» when it is
 *   empty (CR1-3). The key of the printer's own state is not confirmed, so the times decide whether it prints, and a
 *   print that stops counting is called stopped only after 15 s (CR1-4).
 * - A cut link is opened again with growing waits; meanwhile the last state is kept, a print stays active and the
 *   message says «Reconnecting…» (CR1-5).
 * - Sending is not tried: the person is told to turn on Moonraker/Fluidd (CR1-6).
 *
 * The 9999 socket asks for no credentials: anyone on the network can drive the printer (the spec's warning). Only the
 * address the person chose is used, and nothing is ever sent over it.
 */
class CrealityConnection(
    private val host: String,
    private val wsPort: Int,
    private val moonrakerPort: Int,
    private val http: Http,
    /** How long a print that stopped counting stays «printing» before it is called stopped (CR1-4). */
    private val stopWindowMs: Long = 15_000,
) : PrinterConnection {
    private val lock = Lock()
    private var moonraker: PrinterConnection? = null
    private var decided = false
    private var socket: WebSocket? = null
    private var reconnecting = false
    private var closed = false
    private val state = mutableMapOf<String, JsonPrimitive>()
    private var printing = false
    private var stopped = false
    private var stillSince = 0L

    /** Moonraker or the stock socket, decided once (CR1-1). */
    private fun moonraker(): PrinterConnection? {
        if (lock.locked { decided }) return lock.locked { moonraker }
        val m = MoonrakerConnection("http://$host:$moonrakerPort", null, http)
        val works = runCatching { m.test() }.isSuccess
        lock.locked { decided = true; if (works) moonraker = m }
        return if (works) m else null
    }

    private fun ensureSocket() {
        if (lock.locked { socket != null || reconnecting }) return
        val ws = try {
            WebSocket.connect(host, wsPort, "/", readTimeoutMs = 0)
        } catch (e: Throwable) {
            throw PrinterException(PrinterException.Kind.NETWORK, "The printer does not accept state connections at $host.", e,
                PrinterException.Reason.CR_REFUSED)
        }
        lock.locked { socket = ws }
        startDaemon("creality-read") { readLoop(ws) }
    }

    private fun readLoop(ws: WebSocket) {
        try {
            while (true) {
                val text = ws.receive() ?: break
                val o = runCatching { Json.parseToJsonElement(text) as? JsonObject }.getOrNull() ?: continue
                lock.locked { o.forEach { (k, v) -> if (v is JsonPrimitive) state[k] = v } }
            }
        } catch (_: Throwable) {
        }
        runCatching { ws.close() }
        lock.locked { if (socket === ws) socket = null }
        if (!lock.locked { closed }) reconnect()
    }

    /** Opens the socket again with growing waits (1, 2, 4… up to 30 s), the last state kept meanwhile. */
    private fun reconnect() {
        lock.locked { reconnecting = true }
        startDaemon("creality-reconnect") {
            var wait = 1_000L
            while (!lock.locked { closed }) {
                sleepMs(wait)
                val ws = runCatching { WebSocket.connect(host, wsPort, "/", readTimeoutMs = 0) }.getOrNull()
                if (ws != null) {
                    lock.locked { socket = ws; reconnecting = false }
                    startDaemon("creality-read") { readLoop(ws) }
                    return@startDaemon
                }
                wait = (wait * 2).coerceAtMost(30_000)
            }
        }
    }

    override fun test(): String {
        moonraker()?.let { return it.test() }
        ensureSocket()
        val end = nowMs() + 3_000
        while (lock.locked { state.isEmpty() } && nowMs() < end) sleepMs(50)
        return model()
    }

    /** The model, or the board code when the printer gives no model (CR1-3). */
    private fun model(): String = lock.locked {
        val model = state["model"]?.contentOrNull.orEmpty()
        val board = state["modelVersion"]?.contentOrNull.orEmpty()
        when {
            model.isNotBlank() -> if (model.startsWith("Creality", true)) model else "Creality $model"
            board.isNotBlank() -> "Creality (board $board)"
            else -> "Creality"
        }
    }

    override fun status(): PrinterStatus {
        moonraker()?.let { return it.status() }
        ensureSocket()
        return lock.locked {
            val elapsed = state["printJobTime"]?.doubleOrNull?.toLong()
            val left = state["printLeftTime"]?.doubleOrNull?.toLong()
            val now = nowMs()
            if (left != null && left > 0) {
                printing = true; stopped = false; stillSince = 0L
            } else if (printing) {
                // Stopping has no state of its own: the print is called stopped only once the window passes (CR1-4).
                if (stillSince == 0L) stillSince = now
                if (now - stillSince >= stopWindowMs) { printing = false; stopped = true }
            }
            val total = if (elapsed != null && left != null && elapsed + left > 0) elapsed + left else null
            fun temp(a: String, t: String) = state[a]?.doubleOrNull?.let { Temperature(it, state[t]?.doubleOrNull ?: 0.0) }
            PrinterStatus(
                state = if (printing) PrinterState.PRINTING else PrinterState.READY,
                elapsedS = if (printing) elapsed else null,
                remainingS = if (printing) left else null,
                percent = if (printing && total != null && elapsed != null) 100.0 * elapsed / total else null,
                chamber = temp("boxTemp", "targetBoxTemp"),
                message = when {
                    reconnecting -> "Reconnecting…"
                    stopped -> "stopped"
                    else -> ""
                },
                finished = if (stopped) false else null,
                reason = if (reconnecting) PrinterException.Reason.CR_RECONNECTING else null,
            )
        }
    }

    private fun send(): Nothing = throw PrinterException(PrinterException.Kind.UNSUPPORTED,
        "To send to this printer, turn on Moonraker/Fluidd.", reason = PrinterException.Reason.CR_NO_SEND)

    override fun pause() { moonraker()?.pause() ?: send() }

    override fun resume() { moonraker()?.resume() ?: send() }

    override fun cancel() { moonraker()?.cancel() ?: send() }

    override fun upload(name: String, length: Long, start: Boolean, open: () -> ByteInput) {
        moonraker()?.upload(name, length, start, open) ?: send()
    }

    override fun camera(): Camera? = moonraker()?.camera()

    override val hasControls: Boolean get() = lock.locked { moonraker != null }

    override fun setTemperature(heater: Heater, target: Double) { moonraker()?.setTemperature(heater, target) ?: send() }

    override fun home(axes: Set<Axis>) { moonraker()?.home(axes) ?: send() }

    override fun jog(axis: Axis, distance: Double, feedMmS: Double) { moonraker()?.jog(axis, distance, feedMmS) ?: send() }

    override fun extrude(mm: Double, feedMmS: Double) { moonraker()?.extrude(mm, feedMmS) ?: send() }

    override fun close() {
        val ws = lock.locked { closed = true; socket.also { socket = null } }
        ws?.let { runCatching { it.close() } }
        lock.locked { moonraker }?.close()
    }

    companion object {
        const val WS_PORT = 9999
        const val MOONRAKER_PORT = 7125

        /** `creality://host` (tests: `creality://host?ws=9999&moonraker=7125`). */
        fun of(url: String, http: Http, stopWindowMs: Long = 15_000): CrealityConnection {
            val rest = url.removePrefix("creality://").substringAfter("://")
            val host = rest.substringBefore('?').substringBefore('/').substringBefore(':')
            val q = rest.substringAfter('?', "").split('&').mapNotNull { p -> p.split('=', limit = 2).takeIf { it.size == 2 }?.let { it[0] to it[1] } }.toMap()
            return CrealityConnection(host, q["ws"]?.toIntOrNull() ?: WS_PORT, q["moonraker"]?.toIntOrNull() ?: MOONRAKER_PORT, http, stopWindowMs)
        }
    }
}

/** Anycubic printers are not offered: the person is told to use Anycubic's own program (the owner's decision, 17-11). */
class AnycubicConnection : PrinterConnection {
    private fun no(): Nothing = throw PrinterException(PrinterException.Kind.UNSUPPORTED,
        "Use Anycubic Slicer Next for this printer.", reason = PrinterException.Reason.AC_USE_SLICER_NEXT)
    override fun test(): String = no()
    override fun status(): PrinterStatus = no()
    override fun pause() = no()
    override fun resume() = no()
    override fun cancel() = no()
    override fun upload(name: String, length: Long, start: Boolean, open: () -> ByteInput) = no()
    override fun camera(): Camera? = null
    override val hasControls: Boolean get() = false
    override fun setTemperature(heater: Heater, target: Double) = no()
    override fun home(axes: Set<Axis>) = no()
    override fun jog(axis: Axis, distance: Double, feedMmS: Double) = no()
    override fun extrude(mm: Double, feedMmS: Double) = no()
}
