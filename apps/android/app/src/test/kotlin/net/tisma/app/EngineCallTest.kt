package net.tisma.app

import net.tisma.app.engine.Library
import net.tisma.app.engine.Library.LibraryException
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.util.Locale
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** Engine calls: the time limit holds for an engine that hangs, and its errors come in the phone's language. */
class EngineCallTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun engine(script: String): File = tmp.newFile("engine.sh").apply { writeText("#!/bin/sh\n$script\n"); setExecutable(true) }

    @Test(timeout = 30_000)
    fun `an engine that hangs is stopped at the time limit`() {
        val lib = Library(engine("sleep 600"), tmp.newFolder("library"), timeoutS = 2)
        val start = System.currentTimeMillis()
        val e = assertFailsWith<LibraryException> { lib.list() }
        assertTrue("too long" in e.message.orEmpty())
        assertTrue(System.currentTimeMillis() - start < 15_000)
        // No temporary files are left behind.
        assertTrue(tmp.root.listFiles().orEmpty().none { it.name.endsWith(".out") || it.name.endsWith(".err") })
    }

    @Test
    fun `the engine's error is given in the phone's language`() {
        val exe = engine("""echo '{"code":"not_found","error":"No such entry","error_es":"No existe esa entrada"}'; exit 2""")
        val saved = Locale.getDefault()
        try {
            Locale.setDefault(Locale.ENGLISH)
            assertEquals("No such entry", assertFailsWith<LibraryException> { Library(exe, tmp.newFolder("a")).list() }.message)
            Locale.setDefault(Locale.forLanguageTag("es-AR"))
            assertEquals("No existe esa entrada", assertFailsWith<LibraryException> { Library(exe, tmp.newFolder("b")).list() }.message)
        } finally {
            Locale.setDefault(saved)
        }
    }

    @Test(timeout = 30_000)
    fun `a printer profile from an engine that hangs gives up at its limit`() {
        val runner = net.tisma.app.engine.EngineRunner(engine("sleep 600"), tmp.newFolder("work"), profileTimeoutS = 2)
        assertEquals(null, runner.profile(net.tisma.app.engine.EngineRunner.ProfileChoice("generic", 0.4, null)))
    }

    @Test(timeout = 60_000)
    fun `two printer profiles asked at once each get their own`() {
        // The engine writes the printer's name to the -o file, slowly, so the two calls overlap.
        val exe = engine("""out=""; p=""; while [ ${'$'}# -gt 0 ]; do case "${'$'}1" in -o) out="${'$'}2"; shift;; --printer) p="${'$'}2"; shift;; esac; shift; done
sleep 1; echo "{\"printer\":\"${'$'}p\"}" > "${'$'}out"""")
        val runner = net.tisma.app.engine.EngineRunner(exe, tmp.newFolder("work"))
        val names = listOf("a", "b", "c", "d").map { n ->
            java.util.concurrent.CompletableFuture.supplyAsync { runner.profile(net.tisma.app.engine.EngineRunner.ProfileChoice(n, 0.4, null)) }
        }.map { (it.get()?.get("printer") as? kotlinx.serialization.json.JsonPrimitive)?.content }
        assertEquals(listOf("a", "b", "c", "d"), names)
    }
}
