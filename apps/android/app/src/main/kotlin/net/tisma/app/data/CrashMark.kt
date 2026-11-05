package net.tisma.app.data

import java.io.File
import java.io.PrintWriter
import java.io.StringWriter

/**
 * After an unexpected close, the app offers to report it (problem_reports.md,
 * rule 10): when the app dies on an uncaught error, the session's log and the
 * error are written to [file]; the next start finds it, offers the report with
 * «Tisma closed» chosen, and forgets it whatever the person does.
 */
class CrashMark(private val file: File) {
    fun install() {
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            runCatching { record(error) }
            previous?.uncaughtException(thread, error)
        }
    }

    fun record(error: Throwable) {
        val trace = StringWriter().also { error.printStackTrace(PrintWriter(it)) }.toString()
        file.writeText(AppLog.text() + "\n\n" + trace.take(20_000))
    }

    /** The closed session's log, once: the mark is removed as it is read. */
    fun take(): String? {
        if (!file.isFile) return null
        val text = runCatching { file.readText() }.getOrNull()
        file.delete()
        return text
    }
}
