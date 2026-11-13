package net.tisma.app

import net.tisma.app.data.Credits
import org.junit.Test
import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** credits.md rule 5 for the app: every library in the APK has its credit and its notice files exist. */
class CreditsTest {
    private val android = File(System.getProperty("user.dir")).parentFile
    private val root = android.parentFile.parentFile

    private fun credits(spanish: Boolean = false) = Credits.parse(File(root, "resources/credits.json").readText(),
        File(android, "credits_android.json").readText(), spanish)

    @Test
    fun `every library of the catalog that reaches the APK has a credit`() {
        val c = credits()
        val credited = c.libraries.flatMap { it.gradle }.toSet()
        val toml = File(android, "gradle/libs.versions.toml").readText()
        val testOnly = setOf("junit:junit", "org.jetbrains.kotlin:kotlin-test-junit", "org.jetbrains.kotlinx:kotlinx-coroutines-test",
            "org.robolectric:robolectric", "androidx.test:core", "androidx.compose.ui:ui-test-junit4", "androidx.compose.ui:ui-test-manifest",
            // The startup profile's generator (:baselineprofile), on an emulator only.
            "androidx.benchmark:benchmark-macro-junit4", "androidx.test.ext:junit", "androidx.test.uiautomator:uiautomator")
        val modules = Regex("""module\s*=\s*"([^"]+)"""").findAll(toml).map { it.groupValues[1] }.filter { it !in testOnly }.toList()
        val missing = modules.filter { it !in credited }
        assertTrue(missing.isEmpty(), "no credit for: $missing")
        // The engine's libraries the APK links.
        assertEquals(setOf("Clipper2", "nlohmann/json", "Boost.Polygon", "Boost.Config", "oneTBB"),
            c.libraries.map { it.name }.toSet().intersect(setOf("Clipper2", "nlohmann/json", "Boost.Polygon", "Boost.Config", "oneTBB", "GLFW")))
    }

    @Test
    fun `every notice file exists, and the features come from the engine's credits`() {
        val c = credits(spanish = true)
        for (n in c.libraries.flatMap { it.notices }.toSet()) {
            assertTrue(File(root, "resources/licenses/$n").isFile || File(android, "licenses/$n").isFile, "missing notice $n")
        }
        assertTrue(c.features.any { it.name == "Aletas de soporte" && it.license == "MIT" })
    }
}
