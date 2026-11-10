package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.data.AppSettings
import net.tisma.app.engine.EngineRunner
import net.tisma.app.engine.ProfilePack
import net.tisma.app.engine.SliceNotice
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

/** profile_pack_format.md (spec/next b705fe7): a filament that no longer fits the chosen print is replaced, and said. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class FilamentSwitchTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private fun until(c: () -> Boolean) {
        val end = System.currentTimeMillis() + 10_000
        while (!c() && System.currentTimeMillis() < end) { ShadowLooper.idleMainLooper(); Thread.sleep(10) }
    }

    @Test
    fun `changing to a print the filament does not fit switches to one of the same material`() {
        val data = tmp.newFolder("data")
        val pack = File(data, "pack").apply { mkdirs() }
        val printer = "vendors/Acme/models/One.json#0.4"
        File(pack, "index.json").writeText("""{"printers":["$printer"]}""")
        File(pack, "compatible.json").writeText("""{"format":"tisma-pack-compatible","version":2,"sets":[[0]]}""")
        val dir = File(pack, "vendors/Acme/filaments").apply { mkdirs() }
        dir.resolve("Acme_PETG.json").writeText("""{"format":"tisma-pack-entry","technology":"fff","compatible":0,"material":{"type":"PETG"}}""")
        dir.resolve("Acme_PLA.json").writeText("""{"format":"tisma-pack-entry","technology":"fff","compatible":0,"material":{"type":"PLA"}}""")
        dir.resolve("Acme_PLA_Fast.json").writeText("""{"format":"tisma-pack-entry","technology":"fff","compatible":0,"material":{"type":"PLA"},
            "compatible_prints":["vendors/Acme/prints/Fast.json"]}""")
        val context: Context = ApplicationProvider.getApplicationContext()
        val work = tmp.newFolder("work")
        val slice = SliceViewModel(EngineRunner(File(work, "none"), work), work, saved = AppSettings(context),
            pack = ProfilePack(File("none"), data))
        val fast = "vendors/Acme/filaments/Acme_PLA_Fast.json"
        slice.setProfile(EngineRunner.ProfileChoice(printer, 0.4, fast, pack = true, print = "vendors/Acme/prints/Fast.json"))
        slice.loadPackFilaments()
        until { slice.state.value.packFilaments.size == 3 }
        // Still fits: nothing changes.
        slice.choosePackPrint("vendors/Acme/prints/Fast.json")
        ShadowLooper.idleMainLooper()
        assertEquals(fast, slice.state.value.profile?.filament)
        // A print it does not fit: the PLA that fits replaces it, and the person is told which.
        slice.choosePackPrint("vendors/Acme/prints/Fine.json")
        until { slice.state.value.notice is SliceNotice.FilamentSwitched }
        assertEquals("vendors/Acme/filaments/Acme_PLA.json", slice.state.value.profile?.filament)
        assertEquals("vendors/Acme/prints/Fine.json", slice.state.value.profile?.print)
        assertEquals(SliceNotice.FilamentSwitched("Acme PLA"), slice.state.value.notice)
        assertEquals("El filamento no servía con esta impresión: se cambió a Acme PLA",
            context.createConfigurationContext(android.content.res.Configuration(context.resources.configuration)
                .apply { setLocale(java.util.Locale.forLanguageTag("es")) }).getString(R.string.filament_switched, "Acme PLA"))
    }
}
