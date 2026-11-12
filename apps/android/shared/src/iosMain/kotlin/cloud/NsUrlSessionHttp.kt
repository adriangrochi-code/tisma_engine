package net.tisma.cloud

import kotlinx.cinterop.BetaInteropApi
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.usePinned
import net.tisma.io.BytesInput
import net.tisma.io.FileInput
import net.tisma.io.BytesOutput
import net.tisma.io.IOException
import net.tisma.io.copyAll
import platform.Foundation.NSData
import platform.Foundation.NSError
import platform.Foundation.NSFileManager
import platform.Foundation.NSTemporaryDirectory
import platform.Foundation.downloadTaskWithRequest
import platform.Foundation.NSHTTPURLResponse
import platform.Foundation.NSMutableURLRequest
import platform.Foundation.NSURL
import platform.Foundation.NSURLResponse
import platform.Foundation.NSURLSession
import platform.Foundation.NSURLSessionConfiguration
import platform.Foundation.create
import platform.Foundation.dataTaskWithRequest
import platform.Foundation.setHTTPBody
import platform.Foundation.setHTTPMethod
import platform.Foundation.setValue
import platform.darwin.DISPATCH_TIME_FOREVER
import platform.darwin.dispatch_semaphore_create
import platform.darwin.dispatch_semaphore_signal
import platform.darwin.dispatch_semaphore_wait
import platform.posix.memcpy

actual val PLATFORM_NAME: String = "iOS"

actual val PlatformHttp: Http get() = NsUrlSessionHttp

/**
 * HTTP and HTTPS over the iPhone's own client (NSURLSession). Like Android's sender it blocks until the answer is
 * in; callers run it off the main thread. The whole body is built in memory first (a multipart body measured and
 * written by the common code), and the answer is read whole (at most what the session gives): large results go to
 * a file with the download task when the app needs them (apps/ios/PLAN.md, step 2). Redirects are not followed.
 */
@OptIn(ExperimentalForeignApi::class, BetaInteropApi::class)
object NsUrlSessionHttp : Http {
    private val session: NSURLSession by lazy {
        NSURLSession.sessionWithConfiguration(NSURLSessionConfiguration.ephemeralSessionConfiguration)
    }

    override fun send(request: HttpRequest): HttpResponse {
        val url = NSURL.URLWithString(request.url) ?: throw IOException("Not an address: ${request.url}")
        val r = NSMutableURLRequest.requestWithURL(url)
        r.setHTTPMethod(request.method)
        r.setTimeoutInterval(request.timeoutMs / 1000.0)
        request.headers.forEach { (k, v) -> r.setValue(v, forHTTPHeaderField = k) }
        val body: ByteArray? = when {
            request.form.isNotEmpty() -> {
                val boundary = "tisma-" + kotlin.random.Random.nextLong().toULong().toString(16)
                r.setValue("multipart/form-data; boundary=$boundary", forHTTPHeaderField = "Content-Type")
                BytesOutput().also { writeMultipart(it, boundary, request.form) }.toByteArray()
            }
            request.stream != null -> {
                val part = request.stream
                r.setValue(part.contentType, forHTTPHeaderField = "Content-Type")
                BytesOutput().also { out -> part.open().use { it.copyAll(out) } }.toByteArray()
            }
            else -> request.body
        }
        if (body != null) r.setHTTPBody(body.toNSData())

        if (request.large) return sendToFile(r)

        val done = dispatch_semaphore_create(0)
        var data: NSData? = null
        var response: NSURLResponse? = null
        var error: NSError? = null
        session.dataTaskWithRequest(r) { d, resp, e ->
            data = d; response = resp; error = e
            dispatch_semaphore_signal(done)
        }.resume()
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER)

        error?.let { throw IOException(it.localizedDescription) }
        val http = response as? NSHTTPURLResponse ?: throw IOException("No answer from the server")
        val headers = http.allHeaderFields.entries.associate { (k, v) -> k.toString() to v.toString() }
        return HttpResponse(http.statusCode.toInt(), headers, BytesInput(data?.toByteArray() ?: ByteArray(0)))
    }

    /**
     * A large answer (a result of up to 1 GB) goes to a file with a download task, never whole into memory, and is
     * read from there; the file is deleted when the answer is closed.
     */
    private fun sendToFile(r: NSMutableURLRequest): HttpResponse {
        val done = dispatch_semaphore_create(0)
        var path: String? = null
        var response: NSURLResponse? = null
        var error: NSError? = null
        session.downloadTaskWithRequest(r) { location, resp, e ->
            response = resp; error = e
            // The system removes [location] when this returns: the file is moved to a place of ours first.
            if (location != null) {
                val dest = NSTemporaryDirectory() + "tisma-" + kotlin.random.Random.nextLong().toULong().toString(16)
                if (NSFileManager.defaultManager.moveItemAtPath(location.path ?: "", toPath = dest, error = null)) path = dest
            }
            dispatch_semaphore_signal(done)
        }.resume()
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER)

        error?.let { e -> path?.let { remove(it) }; throw IOException(e.localizedDescription) }
        val http = response as? NSHTTPURLResponse ?: run { path?.let { remove(it) }; throw IOException("No answer from the server") }
        val headers = http.allHeaderFields.entries.associate { (k, v) -> k.toString() to v.toString() }
        val file = path ?: return HttpResponse(http.statusCode.toInt(), headers, BytesInput(ByteArray(0)))
        return HttpResponse(http.statusCode.toInt(), headers, FileInput(file, deleteOnClose = true))
    }

    private fun remove(path: String) { NSFileManager.defaultManager.removeItemAtPath(path, error = null) }

    private fun ByteArray.toNSData(): NSData = if (isEmpty()) NSData() else usePinned { p ->
        NSData.create(bytes = p.addressOf(0), length = size.toULong())
    }

    private fun NSData.toByteArray(): ByteArray {
        val n = length.toInt()
        if (n == 0) return ByteArray(0)
        return ByteArray(n).also { b -> b.usePinned { p -> memcpy(p.addressOf(0), bytes, length) } }
    }
}
