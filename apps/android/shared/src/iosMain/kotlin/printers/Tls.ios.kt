package net.tisma.printers

import kotlinx.cinterop.BetaInteropApi
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.ObjCObjectVar
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.alloc
import kotlinx.cinterop.convert
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.reinterpret
import kotlinx.cinterop.usePinned
import kotlinx.cinterop.value
import net.tisma.io.ByteInput
import net.tisma.io.ByteOutput
import net.tisma.io.IOException
import net.tisma.io.Link
import net.tisma.io.Sha256
import net.tisma.io.nowMs
import platform.CoreFoundation.CFDataGetBytePtr
import platform.CoreFoundation.CFDataGetLength
import platform.CoreFoundation.CFRelease
import platform.CoreFoundation.CFStringRef
import platform.CoreFoundation.CFTypeRef
import platform.CFNetwork.kCFStreamPropertySSLPeerTrust
import platform.CFNetwork.kCFStreamPropertySSLSettings
import platform.CFNetwork.kCFStreamSSLValidatesCertificateChain
import platform.Foundation.CFBridgingRelease
import platform.Foundation.CFBridgingRetain
import platform.Foundation.NSInputStream
import platform.Foundation.NSNumber
import platform.Foundation.NSOutputStream
import platform.Foundation.NSStream
import platform.Foundation.NSStreamStatusError
import platform.Foundation.NSStreamStatusOpen
import platform.Foundation.NSThread
import platform.Foundation.getStreamsToHostWithName
import platform.Security.SecCertificateCopyData
import platform.Security.SecCertificateCopySubjectSummary
import platform.Security.SecTrustGetCertificateAtIndex
import platform.Security.SecTrustRef

/**
 * iOS: NSStream sockets with TLS whose chain the system does not check (no public authority signs a printer), and the
 * leaf certificate's SHA-256 compared with the saved one before any byte of ours goes out. The certificate's subject
 * summary is its common name, the printer's serial.
 */
@OptIn(ExperimentalForeignApi::class, BetaInteropApi::class)
actual class PinnedTls actual constructor(private val expectedSha256: String?) {
    actual var seenSha256: String? = null
        private set
    actual var seenName: String? = null
        private set

    actual fun open(host: String, port: Int, timeoutMs: Int, readTimeoutMs: Int): Link {
        val (input, output) = memScoped {
            val i = alloc<ObjCObjectVar<NSInputStream?>>()
            val o = alloc<ObjCObjectVar<NSOutputStream?>>()
            NSStream.getStreamsToHostWithName(host, port.convert(), i.ptr, o.ptr)
            (i.value ?: throw PrinterException(PrinterException.Kind.NETWORK, "Cannot reach $host")) to
                (o.value ?: throw PrinterException(PrinterException.Kind.NETWORK, "Cannot reach $host"))
        }
        val settings = mapOf<Any?, Any?>(key(kCFStreamSSLValidatesCertificateChain) to NSNumber(bool = false))
        input.setProperty(settings, key(kCFStreamPropertySSLSettings))
        output.setProperty(settings, key(kCFStreamPropertySSLSettings))
        input.open(); output.open()

        // The handshake ends when the output has room; the peer's certificate is known from then on.
        val end = nowMs() + timeoutMs
        while (!output.hasSpaceAvailable) {
            if (output.streamStatus == NSStreamStatusError || input.streamStatus == NSStreamStatusError) {
                close(input, output)
                throw PrinterException(PrinterException.Kind.NETWORK, output.streamError?.localizedDescription ?: "TLS failed")
            }
            if (nowMs() > end) { close(input, output); throw PrinterException(PrinterException.Kind.NETWORK, "The printer does not answer") }
            NSThread.sleepForTimeInterval(0.02)
        }
        check(output, input)
        return StreamLink(input, output, readTimeoutMs)
    }

    /**
     * iOS: a new TLS link to the data port with the same pinned certificate. NSStream gives no way to name the session
     * to resume; a printer that insists on resuming it refuses the upload (to be checked on a printer).
     */
    actual fun openData(host: String, port: Int, sessionPort: Int, timeoutMs: Int): Link = open(host, port, timeoutMs, 30_000)

    private fun check(output: NSOutputStream, input: NSInputStream) {
        val trust = output.propertyForKey(key(kCFStreamPropertySSLPeerTrust))
        val trustRef: SecTrustRef? = trust?.let { CFBridgingRetain(it)?.reinterpret() }
        val leaf = trustRef?.let { SecTrustGetCertificateAtIndex(it, 0) }
        if (leaf == null) {
            trustRef?.let { CFRelease(it) }
            close(input, output)
            throw PrinterException(PrinterException.Kind.NETWORK, "The printer sent no certificate")
        }
        val der = SecCertificateCopyData(leaf)
        val bytes = der?.let { d ->
            val n = CFDataGetLength(d).toInt()
            ByteArray(n).also { b -> if (n > 0) b.usePinned { p -> platform.posix.memcpy(p.addressOf(0), CFDataGetBytePtr(d), n.convert()) } }
        } ?: ByteArray(0)
        der?.let { CFRelease(it) }
        val summary = SecCertificateCopySubjectSummary(leaf)
        seenName = summary?.let { CFBridgingRelease(it) as? String }
        trustRef.let { CFRelease(it) }
        val sha = Sha256.fingerprint(bytes)
        seenSha256 = sha
        if (expectedSha256 != null && !expectedSha256.equals(sha, ignoreCase = true)) {
            close(input, output)
            throw PrinterException(PrinterException.Kind.CERTIFICATE, "The printer's certificate is not the one saved: it was not connected")
        }
    }

    /** A CFNetwork constant (a CFString owned by the system) as the String key NSStream takes, without retaining it. */
    private fun key(k: CFStringRef?): String =
        kotlinx.cinterop.interpretObjCPointer<platform.Foundation.NSString>(k!!.rawValue) as String

    private fun close(i: NSInputStream, o: NSOutputStream) { i.close(); o.close() }
}

/** An NSStream pair as a [Link]; reads and writes block, as on Android. */
@OptIn(ExperimentalForeignApi::class)
private class StreamLink(private val i: NSInputStream, private val o: NSOutputStream, private val readTimeoutMs: Int = 0) : Link {
    /** Waits for bytes up to [readTimeoutMs] (0: as long as it takes), as Android's socket timeout does. */
    private fun awaitBytes() {
        if (readTimeoutMs <= 0) return
        val end = nowMs() + readTimeoutMs
        while (!i.hasBytesAvailable) {
            if (i.streamStatus >= platform.Foundation.NSStreamStatusAtEnd) return // the read below says why
            if (nowMs() > end) throw IOException("The printer sent nothing for ${readTimeoutMs / 1000} s")
            NSThread.sleepForTimeInterval(0.01)
        }
    }

    override val input: ByteInput = object : ByteInput() {
        override fun read(): Int {
            val b = ByteArray(1)
            return if (read(b, 0, 1) <= 0) -1 else b[0].toInt() and 0xFF
        }

        override fun read(b: ByteArray, off: Int, len: Int): Int {
            if (len == 0) return 0
            awaitBytes()
            val n = b.usePinned { p -> i.read(p.addressOf(off).reinterpret(), len.convert()) }.toInt()
            if (n < 0) throw IOException(i.streamError?.localizedDescription ?: "The link was cut")
            return if (n == 0) -1 else n
        }

        override fun close() = i.close()
    }

    override val output: ByteOutput = object : ByteOutput() {
        override fun write(b: Int) = write(byteArrayOf(b.toByte()), 0, 1)

        override fun write(b: ByteArray, off: Int, len: Int) {
            var done = 0
            while (done < len) {
                val n = b.usePinned { p -> o.write(p.addressOf(off + done).reinterpret(), (len - done).convert()) }.toInt()
                if (n <= 0) throw IOException(o.streamError?.localizedDescription ?: "The link was cut")
                done += n
            }
        }

        override fun close() = o.close()
    }

    override fun close() { i.close(); o.close() }
}
