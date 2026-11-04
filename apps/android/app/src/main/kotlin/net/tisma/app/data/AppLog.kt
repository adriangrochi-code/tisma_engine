package net.tisma.app.data

import java.time.LocalTime
import java.time.format.DateTimeFormatter

/**
 * The log of this session, in memory: what a problem report can attach
 * (problem_reports.md, rule 3). It never holds secrets: no tokens, passwords
 * or access codes are written to it.
 */
object AppLog {
    private const val MAX_LINES = 500
    private val lines = ArrayDeque<String>()
    private val time = DateTimeFormatter.ofPattern("HH:mm:ss")

    @Synchronized
    fun line(text: String) {
        lines.addLast(LocalTime.now().format(time) + " " + text.replace('\n', ' '))
        while (lines.size > MAX_LINES) lines.removeFirst()
    }

    @Synchronized
    fun text(): String = lines.joinToString("\n")

    @Synchronized
    fun clear() = lines.clear()
}
