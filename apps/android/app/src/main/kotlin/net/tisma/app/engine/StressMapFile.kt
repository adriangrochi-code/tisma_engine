package net.tisma.app.engine

import java.io.File

/** The engine's voxel PLY at [file] (the shared [StressMap.parse]), moved by [offset] to the shown mesh. */
fun StressMap.Companion.read(file: File, offset: FloatArray = floatArrayOf(0f, 0f, 0f), maxFaces: Int = 1_500_000): StressMap =
    file.inputStream().buffered(1 shl 16).use { StressMap.parse(it, offset, maxFaces) }
