package net.tisma.app

import net.tisma.app.data.AppLog
import net.tisma.app.data.CrashMark
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** problem_reports.md, rule 10: after an unexpected close the log is offered once. */
class CrashTest {
    @get:Rule
    val tmp = TemporaryFolder()

    @Test
    fun `a crash leaves the session's log for the next start, once`() {
        val mark = CrashMark(tmp.root.resolve("crash.txt"))
        AppLog.line("engine: sliced in 1200 ms")
        mark.record(IllegalStateException("boom"))
        val log = mark.take()!!
        assertTrue(log.contains("engine: sliced in 1200 ms"))
        assertTrue(log.contains("IllegalStateException: boom"))
        assertNull(mark.take())
    }
}
