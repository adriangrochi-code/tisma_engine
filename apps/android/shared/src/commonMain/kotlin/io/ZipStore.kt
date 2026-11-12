package net.tisma.io

/** One file of a stored ZIP: its name, size, CRC-32 and how to read its bytes. */
class StoredEntry(val name: String, val size: Long, val crc32: Long, val open: () -> ByteInput)

/**
 * Writes a ZIP whose files are stored, not compressed (PKWARE APPNOTE 6.3: 4.3.7 local header, 4.3.12 central
 * directory, 4.3.16 end record), for Bambu Lab's project file. Each entry's size and CRC are known before (no data
 * descriptor). Sizes up to 4 GB (no ZIP64). Own code from the specification.
 */
fun writeStoredZip(out: ByteOutput, entries: List<StoredEntry>) {
    var offset = 0L
    val central = BytesOutput()
    fun le(o: ByteOutput, v: Long, bytes: Int) { for (i in 0 until bytes) o.write(((v ushr (8 * i)) and 0xFF).toInt()) }
    for (e in entries) {
        require(e.size < 0xFFFFFFFFL) { "A file over 4 GB" }
        val name = e.name.encodeToByteArray()
        val head = BytesOutput()
        le(head, 0x04034b50, 4); le(head, 20, 2); le(head, 0x0800, 2) // version, flags: UTF-8 names
        le(head, 0, 2); le(head, 0, 2); le(head, 0x21, 2) // stored; time 00:00:00, date 1980-01-01
        le(head, e.crc32, 4); le(head, e.size, 4); le(head, e.size, 4)
        le(head, name.size.toLong(), 2); le(head, 0, 2)
        head.write(name, 0, name.size)
        val h = head.toByteArray()
        out.write(h, 0, h.size)
        val copied = e.open().use { it.copyAll(out) }
        if (copied != e.size) throw IOException("${e.name} changed while it was packed")
        le(central, 0x02014b50, 4); le(central, 20, 2); le(central, 20, 2); le(central, 0x0800, 2)
        le(central, 0, 2); le(central, 0, 2); le(central, 0x21, 2)
        le(central, e.crc32, 4); le(central, e.size, 4); le(central, e.size, 4)
        le(central, name.size.toLong(), 2); le(central, 0, 2); le(central, 0, 2) // extra, comment
        le(central, 0, 2); le(central, 0, 2); le(central, 0, 4) // disk, internal and external attributes
        le(central, offset, 4)
        central.write(name, 0, name.size)
        offset += h.size + e.size
    }
    val c = central.toByteArray()
    out.write(c, 0, c.size)
    val end = BytesOutput()
    le(end, 0x06054b50, 4); le(end, 0, 2); le(end, 0, 2)
    le(end, entries.size.toLong(), 2); le(end, entries.size.toLong(), 2)
    le(end, c.size.toLong(), 4); le(end, offset, 4); le(end, 0, 2)
    val t = end.toByteArray()
    out.write(t, 0, t.size)
    out.flush()
}
