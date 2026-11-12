package net.tisma.printers

/**
 * iOS: not yet. Packing needs a ZIP writer and an MD5 read in parts over a file (apps/ios/PLAN.md, step 2); until
 * then the iPhone shows a Bambu Lab's state and controls it, and sending a file says so.
 */
actual fun packForBambu(gcode: net.tisma.io.ByteInput): PackedProject =
    throw PrinterException(PrinterException.Kind.UNEXPECTED, "Sending a file to a Bambu Lab from the iPhone is not ready yet")
