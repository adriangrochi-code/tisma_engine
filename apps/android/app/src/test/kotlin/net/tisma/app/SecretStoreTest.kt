package net.tisma.app

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import net.tisma.app.data.KeystoreSecretStore
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import javax.crypto.KeyGenerator
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** A passing Keystore error keeps the person's access codes; only a secret that can never be read again goes. */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SecretStoreTest {
    @Test
    fun `a passing error keeps the secret, another key removes it`() {
        val context: Context = ApplicationProvider.getApplicationContext()
        KeystoreSecretStore(context) { testKey }.put("printer.code", "12345678")
        // The Keystore fails for a moment: nothing is read, nothing is lost.
        val busy = KeystoreSecretStore(context) { throw java.security.ProviderException("Keystore operation failed") }
        assertNull(busy.get("printer.code"))
        assertEquals("12345678", KeystoreSecretStore(context) { testKey }.get("printer.code"))
        // Another key (the data restored on another phone): the secret cannot come back and is removed.
        val other = KeyGenerator.getInstance("AES").apply { init(256) }.generateKey()
        assertNull(KeystoreSecretStore(context) { other }.get("printer.code"))
        assertNull(KeystoreSecretStore(context) { testKey }.get("printer.code"))
    }
}
