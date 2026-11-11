package net.tisma.app.engine

import java.text.Normalizer

/**
 * Text as searches compare it: lower case and without accents, so «deformacion» finds «deformación» and «Boquilla»
 * * finds «boquilla» (Unicode canonical decomposition, combining marks taken out, on both sides of the comparison).
 */
fun fold(s: String): String = Normalizer.normalize(s.lowercase(), Normalizer.Form.NFD).replace(Regex("\\p{Mn}+"), "")

/** The words of a search, folded. */
fun searchWords(query: String): List<String> = fold(query).trim().split(Regex("\\s+")).filter { it.isNotEmpty() }
