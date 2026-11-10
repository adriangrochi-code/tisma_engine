package net.tisma.app.p2p

/**
 * Text a model is published with (p2p_library.md, rule 3 septies; the server refuses it with 422 since tisma-cloud
 * 0221576): no control characters and no bidirectional marks in the name, author, tags or file paths; line breaks
 * only in the description. The form cleans what it can and says what it took out.
 */
object PublishText {
    /** The bidirectional formatting marks (Unicode's Bidi_Control), which can make a name read other than it is. */
    private val BIDI = setOf(0x061C, 0x200E, 0x200F, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x2066, 0x2067, 0x2068, 0x2069)

    data class Cleaned(val text: String, val removed: Int)

    /** [s] without control characters (keeping line breaks when [multiline]) and bidi marks, and how many went. */
    fun clean(s: String, multiline: Boolean = false): Cleaned {
        val out = StringBuilder(s.length)
        var removed = 0
        var i = 0
        while (i < s.length) {
            val c = s[i]
            val bad = (c.code in BIDI) || (c.isISOControl() && !(multiline && (c == '\n' || c == '\r' || c == '\t')))
            if (bad) removed++ else out.append(c)
            i++
        }
        return Cleaned(out.toString(), removed)
    }
}
