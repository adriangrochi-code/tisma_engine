import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import net.tisma.cloud.HttpRequest
import net.tisma.cloud.NsUrlSessionHttp
import net.tisma.io.BytesOutput
import net.tisma.io.FileInput
import net.tisma.io.IOException
import net.tisma.io.copyAll
import platform.Foundation.NSFileManager
import platform.Foundation.NSTemporaryDirectory
import platform.posix.fclose
import platform.posix.fopen
import platform.posix.fwrite
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse

/** A large answer on iOS is read from a file, and the file goes away when the answer is closed. */
@OptIn(ExperimentalForeignApi::class)
class FileInputTest {
    @Test
    fun aFileIsReadWholeAndRemovedOnClose() {
        val path = NSTemporaryDirectory() + "tisma-test-" + kotlin.random.Random.nextInt().toUInt()
        val bytes = ByteArray(200_000) { (it * 31).toByte() }
        val f = fopen(path, "wb")!!
        bytes.usePinned { p -> fwrite(p.addressOf(0), 1u, bytes.size.toULong(), f) }
        fclose(f)
        val out = BytesOutput()
        val input = FileInput(path, deleteOnClose = true)
        assertEquals(bytes.size.toLong(), input.copyAll(out))
        assertEquals(-1, input.read())
        input.close()
        assertContentEquals(bytes, out.toByteArray())
        assertFalse(NSFileManager.defaultManager.fileExistsAtPath(path))
    }

    @Test
    fun aLargeAnswerFromAClosedPortIsANetworkError() {
        assertFailsWith<IOException> {
            NsUrlSessionHttp.send(HttpRequest("GET", "http://127.0.0.1:9/v1/jobs/x/result", timeoutMs = 5_000, large = true))
        }
    }
}
