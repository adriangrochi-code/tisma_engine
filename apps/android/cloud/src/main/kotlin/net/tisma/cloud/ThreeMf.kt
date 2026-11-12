package net.tisma.cloud

import java.io.InputStream
import java.util.zip.ZipInputStream

/** Whether a ZIP holds a 3MF model (a `.model` file under `3D/`): read with java.util.zip, on Android only. */
fun isThreeMfPackage(input: InputStream): Boolean = try {
    ZipInputStream(input).use { zip ->
        generateSequence { zip.nextEntry }.any { e ->
            val name = e.name.replace('\\', '/').trimStart('/')
            !e.isDirectory && name.startsWith("3D/", ignoreCase = true) && name.endsWith(".model", ignoreCase = true)
        }
    }
} catch (e: Exception) {
    false
}

