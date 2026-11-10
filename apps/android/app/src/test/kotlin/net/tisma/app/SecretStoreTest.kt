package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.data.KeystoreSecretStore
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import javax.crypto.KeyGenerator
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SecretStoreTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    private fun appFilesContain(secret: String): Boolean {
        val root = File(context.applicationInfo.dataDir)
        return root.walkTopDown().filter { it.isFile }.any { it.readBytes().toString(Charsets.ISO_8859_1).contains(secret) }
    }

    @Test
    fun `MA8 a stored secret is never in the app's files as text`() {
        val store = KeystoreSecretStore(context) { testKey }
        val token = "tok_9f8e7d6c5b4a39281706"
        store.put("cloud.token", token)
        assertEquals(token, store.get("cloud.token"))
        // SharedPreferences write on a background thread; wait for the file.
        eventually { File(context.applicationInfo.dataDir, "shared_prefs/${KeystoreSecretStore.FILE}.xml").exists() }
        assertFalse(appFilesContain(token))
        store.remove("cloud.token")
        assertNull(store.get("cloud.token"))
    }

    @Test
    fun `a secret sealed with another key reads as gone`() {
        KeystoreSecretStore(context) { testKey }.put("code", "12345678")
        val other = KeyGenerator.getInstance("AES").apply { init(256) }.generateKey()
        assertNull(KeystoreSecretStore(context) { other }.get("code"))
        // And the broken entry was dropped.
        assertNull(KeystoreSecretStore(context) { testKey }.get("code"))
    }
}
