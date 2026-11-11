import net.tisma.cloud.CloudClient
import net.tisma.cloud.FormPart
import net.tisma.cloud.percentEncode
import net.tisma.cloud.HttpResponse
import net.tisma.cloud.multipartLength
import net.tisma.cloud.readUpTo
import net.tisma.cloud.writeMultipart
import net.tisma.io.BytesInput
import net.tisma.io.BytesOutput
import net.tisma.io.IOException
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The common HTTP pieces (multipart body, capped text), on the JVM and the iPhone simulator. */
class HttpCommonTest {
    @Test
    fun multipartLengthMatchesWhatIsWritten() {
        val parts = listOf(
            FormPart.text("params", """{"layer":0.2}""", "application/json"),
            FormPart("file", "model/3mf", "pieza \"ñ\".3mf", -1) { BytesInput(ByteArray(70_000) { it.toByte() }) },
        )
        val out = BytesOutput()
        writeMultipart(out, "b0", parts)
        val body = out.toByteArray()
        assertEquals(multipartLength("b0", parts), body.size.toLong())
        val text = body.decodeToString(0, 200)
        assertTrue("name=\"params\"" in text)
        assertTrue(body.decodeToString().contains("filename=\"pieza %22ñ%22.3mf\""))
        assertTrue(body.decodeToString().endsWith("--b0--\r\n"))
    }

    @Test
    fun answerTextIsCapped() {
        assertEquals("hola", HttpResponse(200, emptyMap(), BytesInput("hola".encodeToByteArray())).text())
        assertFailsWith<IOException> { HttpResponse(200, emptyMap(), BytesInput(ByteArray(11))).text(max = 10) }
        assertEquals(3, BytesInput(ByteArray(9)).readUpTo(3).size)
    }

    @Test
    fun resultNameNeverLeavesItsFolder() {
        assertEquals("a.gcode", CloudClient.filenameOf("attachment; filename=\"../../a.gcode\""))
        assertEquals("b.tea", CloudClient.filenameOf("attachment; filename=b.tea"))
        assertNull(CloudClient.filenameOf(null))
    }

    @Test
    fun idsAndQueryValuesArePercentEncoded() {
        assertEquals("Mi%20tel%C3%A9fono%20%28Android%29", percentEncode("Mi teléfono (Android)"))
        assertEquals("job-1_a.b~c", percentEncode("job-1_a.b~c"))
        assertEquals("a%2F..%2Fb%3Fx%3D1%26y", percentEncode("a/../b?x=1&y"))
    }
}
