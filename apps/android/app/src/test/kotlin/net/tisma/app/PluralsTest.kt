package net.tisma.app

import android.content.Context
import android.content.res.Configuration
import androidx.test.core.app.ApplicationProvider
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.Locale
import kotlin.test.assertEquals

/** Counted texts agree with their number: «1 receta», never «1 recetas». */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class PluralsTest {
    private fun ctx(lang: String): Context {
        val base = ApplicationProvider.getApplicationContext<Context>()
        return base.createConfigurationContext(Configuration(base.resources.configuration).apply { setLocale(Locale.forLanguageTag(lang)) })
    }

    @Test
    fun `one and many, in English and Spanish`() {
        val en = ctx("en").resources
        val es = ctx("es").resources
        assertEquals("1 recipe", en.getQuantityString(R.plurals.library_recipes, 1, 1))
        assertEquals("3 recipes", en.getQuantityString(R.plurals.library_recipes, 3, 3))
        assertEquals("1 receta", es.getQuantityString(R.plurals.library_recipes, 1, 1))
        assertEquals("1 capa", es.getQuantityString(R.plurals.library_layers, 1, 1))
        assertEquals("200 capas", es.getQuantityString(R.plurals.library_layers, 200, 200))
        assertEquals(true, es.getQuantityString(R.plurals.shared_withdrawn, 1, 1).startsWith("1 archivo fue retirado"))
        assertEquals(true, en.getQuantityString(R.plurals.report_similar, 1, 1).contains("1 time before"))
        assertEquals(true, es.getQuantityString(R.plurals.filament_hand_changed, 1, 1).startsWith("Cambiaste 1 ajuste de material"))
    }
}
