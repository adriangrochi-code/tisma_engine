package net.tisma.app.engine

import java.io.File

/** Reads `<dir>` (the pack's `knowledge` folder) with the shared [Knowledge.parse]; an empty help when it is not there. */
fun Knowledge.Companion.read(dir: File): Knowledge =
    Knowledge.parse { name -> File(dir, name).takeIf { it.isFile }?.readText() }
