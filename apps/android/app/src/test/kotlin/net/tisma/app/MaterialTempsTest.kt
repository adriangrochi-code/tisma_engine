package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.data.AppSettings
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.SettingsLayout
import net.tisma.app.engine.SliceViewModel
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLooper
import java.io.File
import kotlin.test.assertEquals

/** QA AND11: the Material tab's temperatures come from the chosen filament (the built-in catalog, offline). */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class MaterialTempsTest {
    @get:Rule
    val tmp = TemporaryFolder()

    /** The host engine when given; otherwise a stand-in whose `profile` writes those values. */
    private fun engine(): File = System.getenv("TISMA_HOST_ENGINE")?.let(::File)?.takeIf { it.canExecute() }
        ?: tmp.newFile("engine.sh").apply {
            writeText("#!/bin/sh\nwhile [ $# -gt 0 ]; do [ \"$1\" = -o ] && out=\"$2\"; shift; done\n" +
                "echo '{\"machine\":{\"variables\":{\"temperature\":260,\"bed_temperature\":90}}}' > \"\$out\"\n")
            setExecutable(true)
        }

    @Test
    fun `AND11 a catalog printer and filament give the filament's temperatures`() {
        val exe = engine()
        val context: Context = ApplicationProvider.getApplicationContext()
        // A fresh install: the engine's working folder in the cache does not exist yet.
        val work = File(tmp.root, "cache/engine")
        val slice = SliceViewModel(EngineRunner(exe, work), work, saved = AppSettings(context),
            loadLayout = { SettingsLayout.parse(File("src/main/assets/settings/layout_en.json").readText()) })
        slice.setProfile(EngineRunner.ProfileChoice("Anycubic Kobra 2", 0.4, "Anycubic ABS"))
        val end = System.currentTimeMillis() + 20_000
        while (slice.state.value.profileValues.isEmpty() && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(50) }
        val s = slice.state.value
        System.err.println("VALUES " + s.profileValues.size + " " + s.profileValues["machine.variables.temperature"])
        assertEquals("260", SettingsLayout.show(s.value("machine.variables.temperature")))
        assertEquals("260", SettingsLayout.show(s.value("machine.variables.first_layer_temperature")))
        assertEquals("90", SettingsLayout.show(s.value("machine.variables.bed_temperature")))
    }
}
