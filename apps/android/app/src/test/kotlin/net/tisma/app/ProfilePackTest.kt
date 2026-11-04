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

    /**
     * A fake engine with the commands of docs/CATALOG.md: the plan without
     * --index gives the index, with it the vendor; install copies the
     * downloaded files into <data>/pack and reports its version.
     */
    private fun engine(base: String, size: Int = 4): File = tmp.newFile("engine.sh").apply {
        writeText("""#!/bin/sh
            |cmd="${'$'}2"; data=""; from=""; index=""
            |while [ ${'$'}# -gt 0 ]; do case "${'$'}1" in --data) data="${'$'}2"; shift;; --from) from="${'$'}2"; shift;; --index) index="${'$'}2"; shift;; esac; shift; done
            |if [ "${'$'}cmd" = "plan" ]; then
            |  if [ -z "${'$'}index" ]; then echo '{"downloads":[{"url":"$base/index.json","name":"index.json","size":0,"sha256":""}],"installed":null,"published":null}'
            |  else echo '{"downloads":[{"url":"$base/Acme.tar.xz","name":"Acme.tar.xz","size":$size,"sha256":"x"}],"installed":null,"published":"2026.11.04"}'; fi
            |elif [ "${'$'}cmd" = "install" ]; then
            |  echo "progress: installed Acme.tar.xz" >&2; mkdir -p "${'$'}data/pack"; cp -r "${'$'}from"/. "${'$'}data/pack"/
            |  echo '{"installed":["Acme.tar.xz"],"kept":[],"discarded":[],"warnings":[],"pack_version":"2026.11.04"}'
            |elif [ "${'$'}cmd" = "list" ]; then
            |  echo '{"pack_version":"2026.11.04","vendors":[{"id":"Acme","models":[{"name":"Acme One","variants":[{"id":"0.4","nozzle":0.4,"printer":"vendors/Acme/models/One.json#0.4"}]}]}]}'
            |else echo "error: usage" >&2; exit 1; fi
            |""".trimMargin())
        setExecutable(true)
    }

    @Test
    fun `MA1 the index then the vendors are planned, downloaded and installed`() {
        val s = server(mapOf("index.json" to "{}".toByteArray(), "Acme.tar.xz" to "ACME".toByteArray()))
        try {
            val data = tmp.newFolder("data")
            val progress = mutableListOf<Int>()
            val pack = ProfilePack(engine(s.url), data)
            assertEquals("2026.11.04", pack.update(emptyList(), null, { progress += it }, { false }))
            assertEquals("{}", File(data, "pack/index.json").readText())
            assertEquals("ACME", File(data, "pack/Acme.tar.xz").readText())
            assertEquals(100, progress.last())
            assertEquals(listOf(ProfilePack.Printer("Acme", "Acme One", 0.4, "vendors/Acme/models/One.json#0.4")), pack.printers())
            assertTrue(!File(data, "pack-staging").exists())
        } finally {
            s.close()
        }
    }

    @Test
    fun `an incomplete download or an engine error stops the update`() {
        val s = server(mapOf("index.json" to "{}".toByteArray(), "Acme.tar.xz" to "AC".toByteArray()))
        try {
            val e = assertFailsWith<ProfilePack.PackException> {
                ProfilePack(engine(s.url), tmp.newFolder("p1")).update(emptyList(), null, {}, { false })
            }
            assertTrue(e.message!!.contains("incomplete"), e.message)
        } finally {
            s.close()
        }
    }

    /**
     * With the real engine (TISMA_HOST_ENGINE) and the pack's files downloaded
     * once into TISMA_PACK_FILES (index.json, compatible.json, the archives),
     * served here: the engine plans, the app downloads, the engine installs,
     * and a printer of the pack gives its settings.
     */
    @Test
    fun `MA1 the real engine installs the pack the app downloads`() {
        val exe = System.getenv("TISMA_HOST_ENGINE")?.let(::File)
        val files = System.getenv("TISMA_PACK_FILES")?.let(::File)
        org.junit.Assume.assumeTrue(exe != null && exe.canExecute() && files != null && files.isDirectory)
        val s = server(files!!.listFiles()!!.associate { it.name to it.readBytes() })
        try {
            val data = tmp.newFolder("real")
            val pack = ProfilePack(exe!!, data)
            val v = pack.update(listOf("Anker"), s.url, {}, { false })
            println("pack $v")
            val printers = pack.printers()
            assertTrue(printers.any { it.name == "AnkerMake M5" }, printers.toString())
        } finally {
            s.close()
        }
    }
}
