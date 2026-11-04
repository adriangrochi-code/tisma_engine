package net.tisma.app

import net.tisma.app.data.SecretStore
import java.io.ByteArrayOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey

/** A software AES key in place of the Android Keystore, which Robolectric does not have. */
val testKey: SecretKey by lazy { KeyGenerator.getInstance("AES").apply { init(256) }.generateKey() }

class MemorySecrets : SecretStore {
    val values = mutableMapOf<String, String>()
    override fun get(name: String) = values[name]
    override fun put(name: String, value: String) { values[name] = value }
    override fun remove(name: String) { values.remove(name) }
}

/** Waits (up to [timeoutMs]) for [condition], letting the main looper run. */
fun eventually(timeoutMs: Long = 10_000, condition: () -> Boolean) {
    val end = System.currentTimeMillis() + timeoutMs
    while (System.currentTimeMillis() < end) {
        org.robolectric.shadows.ShadowLooper.idleMainLooper()
        if (condition()) return
        Thread.sleep(10)
    }
    throw AssertionError("condition not met in $timeoutMs ms")
}

fun cube3mf(): ByteArray {
    val out = ByteArrayOutputStream()
    ZipOutputStream(out).use { z ->
        z.putNextEntry(ZipEntry("[Content_Types].xml"))
        z.write("<Types/>".toByteArray())
        z.putNextEntry(ZipEntry("3D/3dmodel.model"))
        z.write("<model unit=\"millimeter\"><resources/><build/></model>".toByteArray())
    }
    return out.toByteArray()
}
