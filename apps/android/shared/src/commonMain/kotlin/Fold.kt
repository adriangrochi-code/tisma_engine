package net.tisma.app.p2p

/**
 * Text as a search compares it, on Android and iOS alike: lower case and without the accents of the Latin letters
 * people type in Spanish, Portuguese, French, German, Italian and Catalan («deformacion» finds «Deformación»).
 * Common code has no Unicode decomposition, so the letters are listed (own table, Latin-1 and Latin Extended-A).
 */
fun foldText(s: String): String {
    val out = StringBuilder(s.length)
    for (c in s.lowercase()) out.append(FOLD[c] ?: c)
    return out.toString()
}

private val FOLD: Map<Char, Char> = buildMap {
    fun put(from: String, to: Char) = from.forEach { put(it, to) }
    put("àáâãäåāăą", 'a'); put("çćĉċč", 'c'); put("ďđ", 'd'); put("èéêëēĕėęě", 'e'); put("ĝğġģ", 'g')
    put("ĥħ", 'h'); put("ìíîïĩīĭįı", 'i'); put("ĵ", 'j'); put("ķ", 'k'); put("ĺļľŀł", 'l'); put("ñńņňŉ", 'n')
    put("òóôõöøōŏő", 'o'); put("ŕŗř", 'r'); put("śŝşš", 's'); put("ţťŧ", 't'); put("ùúûüũūŭůűų", 'u')
    put("ŵ", 'w'); put("ýÿŷ", 'y'); put("źżž", 'z')
}
