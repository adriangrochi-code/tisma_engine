package net.tisma.cloud

import java.io.IOException
import java.io.InputStream
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

/** An answer that never ends is refused at the limit, not read into memory. */
class HttpLimitTest {
    private class Endless : InputStream() {
        override fun read() = 'x'.code
        override fun read(b: ByteArray, off: Int, len: Int): Int { b.fill('x'.code.toByte(), off, off + len); return len }
    }

    @Test(timeout = 20_000)
    fun `an endless body stops at the limit`() {
        assertFailsWith<IOException> { HttpResponse(200, emptyMap(), Endless()).text() }
        assertEquals("ok", HttpResponse(200, emptyMap(), "ok".byteInputStream()).text())
        assertFailsWith<IOException> { HttpResponse(200, emptyMap(), "toolong".byteInputStream()).text(max = 3) }
    }
}
