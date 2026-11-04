package net.tisma.app

import net.tisma.app.engine.ProfilePack
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** The app's side of `pack plan` / `pack install` against a fake engine and a local server. */
class ProfilePackTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun server(files: Map<String, ByteArray>) = net.tisma.mockcloud.StaticFiles(files)

    /** A fake engine: the first plan lists the index, the second the vendor; install copies staging into data. */
    private fun engine(base: String): File = tmp.newFile("engine.sh").apply {
        writeText("""#!/bin/sh
            |cmd="${'$'}2"; data=""; from=""
            |while [ ${'$'}# -gt 0 ]; do case "${'$'}1" in --data) data="${'$'}2"; shift;; --from) from="${'$'}2"; shift;; esac; shift; done
            |if [ "${'$'}cmd" = "plan" ]; then
            |  if [ -f "${'$'}data/index.json" ]; then
            |    if [ -d "${'$'}data/vendors" ]; then echo '[]'; else echo '[{"url":"$base/vendors/acme.zip","path":"vendors/acme.zip","size":4,"sha256":"x"}]'; fi
            |  else echo '[{"url":"$base/index.json","path":"index.json","size":2,"sha256":"x"}]'; fi
            |elif [ "${'$'}cmd" = "install" ]; then
            |  echo "progress 50"; cp -r "${'$'}from"/. "${'$'}data"/
            |else echo "usage" >&2; exit 1; fi
            |""".trimMargin())
        setExecutable(true)
    }

    @Test
    fun `MA1 the index then the vendors are planned, downloaded and installed`() {
        val s = server(mapOf("index.json" to "{}".toByteArray(), "vendors/acme.zip" to "ACME".toByteArray()))
        try {
            val data = tmp.newFolder("pack")
            val progress = mutableListOf<Int>()
            ProfilePack(engine(s.url), data).update(listOf("acme"), null, { progress += it }, { false })
            assertEquals("{}", File(data, "index.json").readText())
            assertEquals("ACME", File(data, "vendors/acme.zip").readText())
            assertEquals(100, progress.last())
        } finally {
            s.close()
        }
    }

    @Test
    fun `an incomplete download or an engine error stops the update`() {
        val s = server(mapOf("index.json" to "{".toByteArray()))
        try {
            val e = assertFailsWith<ProfilePack.PackException> {
                ProfilePack(engine(s.url), tmp.newFolder("p1")).update(emptyList(), null, {}, { false })
            }
            assertTrue(e.message!!.contains("incomplete"))
        } finally {
            s.close()
        }
    }
}
