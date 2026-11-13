package net.tisma.baselineprofile

import androidx.benchmark.macro.junit4.BaselineProfileRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.uiautomator.By
import androidx.test.uiautomator.Until
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

/**
 * The path a person takes at every start (QA, 13-11-2026): the app from cold to its first screen, then the plate
 * and the other sections of the bottom bar, so their first drawing is compiled ahead of time too. Own design over
 * the public API of androidx.benchmark (BaselineProfileRule).
 */
@RunWith(AndroidJUnit4::class)
class StartupProfile {
    @get:Rule
    val rule = BaselineProfileRule()

    @Test
    fun startup() = rule.collect(packageName = "net.tismaservidores.tisma", includeInStartupProfile = true) {
        pressHome()
        startActivityAndWait()
        device.waitForIdle()
        for (tab in listOf("Plate", "Placa", "Printer", "Impresora", "Engineering", "Ingeniería", "More", "Más", "Library", "Biblioteca")) {
            device.findObject(By.text(tab))?.click()
            device.wait(Until.hasObject(By.pkg(packageName)), 2_000)
            device.waitForIdle()
        }
    }
}
