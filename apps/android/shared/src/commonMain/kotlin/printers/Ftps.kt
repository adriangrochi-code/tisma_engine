package net.tisma.printers

import net.tisma.io.ByteInput
import net.tisma.io.ByteOutput
import net.tisma.io.copyAll

/**
 * Uploads one file by implicit FTPS (FTP, RFC 959, over TLS from the first
 * byte, RFC 4217's PBSZ and PROT P), as Bambu Lab's local mode wants it on port
 * 990 (printer_connections.md, «Bambu Lab en red local», rule 5). Own code from
 * the RFCs. The data connection reuses the control connection's TLS session where the platform can (Android; on
 * iOS not checked on a printer yet). Common code.
 */
class FtpsUpload(private val tls: PinnedTls, private val host: String, private val port: Int) {

    fun store(user: String, password: String, remoteName: String, length: Long, open: () -> ByteInput) {
        val control = tls.open(host, port, readTimeoutMs = 30_000)
        try {
            val r = Lines(control.input)
            val w = control.output
            expect(r, 220)
            command(w, "USER $user"); expect(r, 331, 230)
            command(w, "PASS $password")
            val login = reply(r)
            if (login.first != 230) throw PrinterException(PrinterException.Kind.AUTH, "The printer refused the access code")
            command(w, "PBSZ 0"); expect(r, 200)
            command(w, "PROT P"); expect(r, 200)
            command(w, "TYPE I"); expect(r, 200)
            command(w, "PASV")
            val pasv = expect(r, 227)
            val nums = Regex("""(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)""").find(pasv)?.groupValues?.drop(1)?.map { it.toInt() }
                ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "Bad PASV answer")
            val dataPort = nums[4] * 256 + nums[5]
            command(w, "STOR $remoteName")
            // The data address is the printer's own (the PASV address may be a private one behind it).
            val data = tls.openData(host, dataPort, port)
            try {
                expect(r, 150, 125)
                open().use { it.copyAll(data.output) }
                data.output.flush()
            } finally {
                data.close()
            }
            expect(r, 226, 250)
            runCatching { command(w, "QUIT") }
        } finally {
            runCatching { control.close() }
        }
        if (length < 0) Unit
    }

    private fun command(w: ByteOutput, line: String) {
        val b = (line + "\r\n").encodeToByteArray()
        w.write(b, 0, b.size)
        w.flush()
    }

    /** Reply lines, read byte by byte so nothing of the stream is taken ahead. */
    private class Lines(private val input: ByteInput) {
        fun readLine(): String? {
            val b = net.tisma.io.BytesOutput()
            while (true) {
                val c = input.read()
                if (c < 0) return if (b.toByteArray().isEmpty()) null else b.toByteArray().decodeToString()
                if (c == '\n'.code) return b.toByteArray().decodeToString().removeSuffix("\r")
                b.write(c)
                if (b.toByteArray().size > 8192) throw PrinterException(PrinterException.Kind.UNEXPECTED, "An FTP reply too long")
            }
        }
    }

    /** A reply: its code and text, multi-line replies read to their last line (RFC 959, 4.2). */
    private fun reply(r: Lines): Pair<Int, String> {
        var line = r.readLine() ?: throw PrinterException(PrinterException.Kind.NETWORK, "The printer closed the connection")
        val code = line.take(3).toIntOrNull() ?: throw PrinterException(PrinterException.Kind.UNEXPECTED, "Bad FTP reply")
        if (line.length > 3 && line[3] == '-') {
            while (true) {
                line = r.readLine() ?: break
                if (line.startsWith("$code ")) break
            }
        }
        return code to line
    }

    private fun expect(r: Lines, vararg codes: Int): String {
        val (code, text) = reply(r)
        if (code !in codes) throw PrinterException(PrinterException.Kind.SERVER, "FTP: $text")
        return text
    }
}
