package net.tisma.app.p2p

import kotlin.test.Test
import kotlin.test.assertEquals

/** What the server refuses with 422 never leaves the form. */
class PublishTextTest {
    @Test
    fun controlCharactersAndBidiMarksGo() {
        // «gnp.exe» reversed by U+202E would read as a picture's name.
        assertEquals(PublishText.Cleaned("Soporteexe.gnp", 1), PublishText.clean("Soporte‮exe.gnp"))
        assertEquals(PublishText.Cleaned("ab", 2), PublishText.clean("a\u0000\nb"))
        assertEquals(PublishText.Cleaned("Línea 1\nLínea 2", 1), PublishText.clean("Línea 1\nLínea⁦ 2", multiline = true))
        assertEquals(PublishText.Cleaned("Engranaje · ñandú", 0), PublishText.clean("Engranaje · ñandú"))
    }
}
