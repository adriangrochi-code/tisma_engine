package net.tisma.printers

import java.io.BufferedReader
import java.io.InputStream
import java.io.InputStreamReader
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket

/**
 * Uploads one file by implicit FTPS (FTP, RFC 959, over TLS from the first
 * byte, RFC 4217's PBSZ and PROT P), as Bambu Lab's local mode wants it on port
 * 990 (printer_connections.md, «Bambu Lab en red local», rule 5). Own code from
 * the RFCs. The data connection reuses the control connection's TLS session.
 */
class FtpsUpload(private val tls: PinnedTls, private val host: String, private val port: Int) {

    fun store(user: String, password: String, remoteName: String, length: Long, open: () -> InputStream) {
        val control = tls.socket(host, port)
        control.soTimeout = 30_000
        control.use {
            val r = BufferedReader(InputStreamReader(control.getInputStream(), Charsets.UTF_8))
            val w = control.getOutputStream()
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
            val plain = Socket().apply { connect(InetSocketAddress(host, dataPort), 10_000) }
            val data = tls.wrap(plain, host, port)
            expect(r, 150, 125)
            data.use { d -> open().use { it.copyTo(d.getOutputStream(), 64 * 1024) } }
            expect(r, 226, 250)
            runCatching { command(w, "QUIT") }
        }
        if (length < 0) Unit
    }

    private fun command(w: OutputStream, line: String) {
        w.write((line + "\r\n").toByteArray(Charsets.UTF_8))
        w.flush()
    }

    /** A reply: its code and text, multi-line replies read to their last line (RFC 959, 4.2). */
    private fun reply(r: BufferedReader): Pair<Int, String> {
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

    private fun expect(r: BufferedReader, vararg codes: Int): String {
        val (code, text) = reply(r)
        if (code !in codes) throw PrinterException(PrinterException.Kind.SERVER, "FTP: $text")
        return text
    }
}
