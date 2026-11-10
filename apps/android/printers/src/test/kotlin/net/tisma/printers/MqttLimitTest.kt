package net.tisma.printers

import java.io.IOException
import java.net.ServerSocket
import java.net.Socket
import kotlin.concurrent.thread
import kotlin.test.Test
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** A peer that announces a huge packet is refused at once, without taking the memory. */
class MqttLimitTest {
    @Test(timeout = 10_000)
    fun `a 200 MB packet length is refused`() {
        ServerSocket(0).use { server ->
            thread(isDaemon = true) {
                server.accept().use { c ->
                    MqttClient.readFrom(c.getInputStream()) // CONNECT
                    // CONNACK header claiming 200 MB (varint 0x80 0x80 0xD0 0x5F), then nothing.
                    c.getOutputStream().apply { write(byteArrayOf(0x20, 0x80.toByte(), 0x80.toByte(), 0xD0.toByte(), 0x5F)); flush() }
                    Thread.sleep(5_000)
                }
            }
            val client = MqttClient(Socket("127.0.0.1", server.localPort), "t", { _, _ -> })
            val e = assertFailsWith<IOException> { client.connect("bblp", "code") }
            assertTrue("too big" in e.message.orEmpty())
        }
    }
}
