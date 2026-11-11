package net.tisma.printers

// Bambu Lab needs TLS sockets with the printer's own certificate (JVM here; Network.framework on iOS later,
// apps/ios/PLAN.md step 2), so opening a saved printer stays in this module.

/** Opens the connection of a saved printer, with its key or password (null when it needs none). */
fun PrinterConfig.connect(secret: String?, http: net.tisma.cloud.Http = net.tisma.cloud.UrlConnectionHttp): PrinterConnection =
    when (kind) {
        PrinterKind.MOONRAKER -> MoonrakerConnection(url, secret, http)
        PrinterKind.OCTOPRINT -> OctoPrintConnection(url, secret.orEmpty(), http)
        PrinterKind.PRUSALINK -> PrusaLinkConnection(url, user.ifBlank { "maker" }, secret.orEmpty(), http)
        PrinterKind.DUET -> DuetConnection(url, secret.orEmpty().ifEmpty { "reprap" }, http)
        PrinterKind.BAMBU -> BambuConnection.of(url, secret.orEmpty(), certSha256.ifBlank { null })
    }

