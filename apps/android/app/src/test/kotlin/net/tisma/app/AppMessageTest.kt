package net.tisma.app

import android.content.Context
import android.content.res.Configuration
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.ui.appMessage
import net.tisma.app.ui.jobCodeText
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.Locale
import kotlin.test.assertEquals

/** The app's own messages about the engine reach a Spanish phone in Spanish; the engine's words pass as they come. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AppMessageTest {
    private val es: Context = ApplicationProvider.getApplicationContext<Context>().let { c ->
        c.createConfigurationContext(Configuration(c.resources.configuration).apply { setLocale(Locale.forLanguageTag("es")) })
    }

    @Test
    fun `engine messages in Spanish`() {
        assertEquals("El motor tardó demasiado y se detuvo.", es.appMessage("The engine took too long"))
        assertEquals("El motor tardó demasiado y se detuvo.", es.appMessage("The design took too long"))
        assertEquals("El motor se detuvo (código 3).", es.appMessage("The engine stopped (exit code 3)"))
        assertEquals("El motor se detuvo (código 2).", es.appMessage("The engine stopped (2)"))
        assertEquals("Prusa.tar.xz llegó incompleto: probá de nuevo.", es.appMessage("Prusa.tar.xz arrived incomplete"))
        assertEquals("El motor dio una respuesta que la app no puede leer.", es.appMessage("Unexpected answer from the engine"))
        assertEquals("El motor tardó demasiado y se detuvo.", es.jobCodeText("timeout"))
        // The engine's own sentence is not touched.
        assertEquals("no such entry: e9", es.appMessage("no such entry: e9"))
    }
}
