package net.tisma.app.printers

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import net.tisma.app.MainActivity
import net.tisma.app.R
import net.tisma.app.TismaApplication
import net.tisma.printers.PrintEvent
import net.tisma.printers.PrintEvents
import net.tisma.printers.PrinterException
import net.tisma.printers.PrinterStatus
import net.tisma.printers.connect

/**
 * Follows the saved printers while one of them prints, so the person is told
 * when a print ends, pauses (for example, out of filament) or fails, with
 * the app closed (docs/MOBILE.md, «Avisos»). It stops by itself when nothing
 * prints. A foreground service: Android shows its own small notification.
 */
class PrintWatchService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var started = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        channels(this)
        val ongoing = NotificationCompat.Builder(this, CHANNEL_ONGOING)
            .setSmallIcon(R.drawable.tisma_printer)
            .setContentTitle(getString(R.string.watch_title))
            .setContentText(getString(R.string.watch_text))
            .setOngoing(true)
            .setContentIntent(openApp(this))
            .build()
        ServiceCompat.startForeground(this, ONGOING_ID, ongoing,
            if (Build.VERSION.SDK_INT >= 29) ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE else 0)
        if (!started) {
            started = true
            scope.launch { watch() }
        }
        return START_STICKY
    }

    private suspend fun watch() {
        val app = application as TismaApplication
        val store = PrinterStore(app, app.secrets)
        val last = mutableMapOf<String, PrinterStatus>()
        val open = mutableMapOf<String, Pair<net.tisma.printers.PrinterConfig, net.tisma.printers.PrinterConnection>>()
        var quiet = 0
        while (scope.isActive) {
            var anyActive = false
            for (p in store.all()) {
                val conn = open[p.id]?.takeIf { it.first == p }?.second
                    ?: p.connect(store.secret(p.id)).also { c -> open.put(p.id, p to c)?.second?.close() }
                val now = try {
                    conn.status()
                } catch (e: PrinterException) {
                    null
                }
                PrintEvents.between(last[p.id], now)?.let { notify(this, p.name, it) }
                if (now != null) last[p.id] = now
                if (PrintEvents.isActive(now ?: last[p.id])) anyActive = true
            }
            // Two quiet rounds in a row (no print on any printer): done.
            quiet = if (anyActive) 0 else quiet + 1
            if (quiet >= 2) break
            delay(POLL_MS)
        }
        open.values.forEach { it.second.close() }
        ServiceCompat.stopForeground(this, ServiceCompat.STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    override fun onDestroy() {
        scope.cancel()
        super.onDestroy()
    }

    companion object {
        const val POLL_MS = 15_000L
        private const val CHANNEL_ONGOING = "print_watch"
        private const val CHANNEL_EVENTS = "print_events"
        private const val ONGOING_ID = 1

        /** Starts following the printers (called when a print is seen or sent). */
        fun start(context: Context) {
            runCatching { ContextCompat.startForegroundService(context, Intent(context, PrintWatchService::class.java)) }
        }

        fun channels(context: Context) {
            val nm = context.getSystemService(NotificationManager::class.java)
            nm.createNotificationChannel(NotificationChannel(CHANNEL_ONGOING, context.getString(R.string.watch_channel),
                NotificationManager.IMPORTANCE_LOW))
            nm.createNotificationChannel(NotificationChannel(CHANNEL_EVENTS, context.getString(R.string.events_channel),
                NotificationManager.IMPORTANCE_HIGH))
        }

        fun text(context: Context, printer: String, e: PrintEvent): Pair<String, String> = when (e) {
            is PrintEvent.Finished -> context.getString(R.string.event_finished, printer) to e.file
            // The printer's own word for its state, when it gave one (a Bambu Lab's, shown as it comes).
            is PrintEvent.Stopped -> context.getString(R.string.event_stopped, printer) to
                (if (e.state.isBlank()) e.file else context.getString(R.string.event_stopped_state, e.file, e.state))
            is PrintEvent.Paused -> context.getString(R.string.event_paused, printer) to e.message.ifBlank { e.file }
            is PrintEvent.Failed -> context.getString(R.string.event_failed, printer) to e.message.ifBlank { e.file }
        }

        fun notify(context: Context, printer: String, e: PrintEvent) {
            val (title, body) = text(context, printer, e)
            val n: Notification = NotificationCompat.Builder(context, CHANNEL_EVENTS)
                .setSmallIcon(R.drawable.tisma_printer)
                .setContentTitle(title)
                .setContentText(body)
                .setAutoCancel(true)
                .setContentIntent(openApp(context))
                .build()
            // Without the permission (Android 13+) the notice is simply not shown.
            if (Build.VERSION.SDK_INT >= 33 && ContextCompat.checkSelfPermission(context,
                    android.Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
            runCatching { NotificationManagerCompat.from(context).notify((printer + e.file).hashCode(), n) }
        }

        private fun openApp(context: Context): PendingIntent = PendingIntent.getActivity(context, 0,
            Intent(context, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
    }
}
