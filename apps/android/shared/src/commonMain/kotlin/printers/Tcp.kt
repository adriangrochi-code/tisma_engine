package net.tisma.printers

/**
 * A plain TCP link to a printer on the local network (Flashforge's port 8899, printer_connections.md, «Flashforge en
 * red local», TCP). A refused or silent connection is a [PrinterException] of kind NETWORK after [timeoutMs]; a read
 * that waits longer than [readTimeoutMs] (0: none) fails the same way.
 */
expect fun openTcp(host: String, port: Int, timeoutMs: Int = 10_000, readTimeoutMs: Int = 10_000): net.tisma.io.Link
