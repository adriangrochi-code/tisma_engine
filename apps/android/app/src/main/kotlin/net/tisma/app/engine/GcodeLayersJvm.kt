package net.tisma.app.engine

import java.io.BufferedReader

/** Android: the preview reads a G-code file line by line, never whole in memory. */
fun GcodeLayers.Companion.read(reader: BufferedReader): GcodeLayers = read(reader.lineSequence())
