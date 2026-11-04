package net.tisma.app.data

import android.content.Context
import android.content.SharedPreferences
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * Secrets of the app: the Tisma Cloud token (cloud.md, rule 3) and, later,
 * printer access codes (mobile_app.md, rule 14). Never kept as plain text.
 */
interface SecretStore {
    fun get(name: String): String?
    fun put(name: String, value: String)
    fun remove(name: String)
}

/**
 * Keeps each secret encrypted with AES-256-GCM under a key that lives in the
 * Android Keystore and never leaves it; only the encrypted bytes and their
 * nonce are written to the app's preferences. [key] is replaceable so the
 * tests (where there is no Keystore) can give their own.
 */
class KeystoreSecretStore(
    context: Context,
    private val key: () -> SecretKey = { keystoreKey() },
) : SecretStore {
    private val prefs: SharedPreferences = context.getSharedPreferences(FILE, Context.MODE_PRIVATE)

    override fun get(name: String): String? {
        val stored = prefs.getString(name, null) ?: return null
        return try {
            val bytes = Base64.decode(stored, Base64.NO_WRAP)
            val cipher = Cipher.getInstance(TRANSFORMATION)
            cipher.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(128, bytes, 0, NONCE))
            String(cipher.doFinal(bytes, NONCE, bytes.size - NONCE), Charsets.UTF_8)
        } catch (e: Exception) {
            // A key that was lost (the app's data was restored elsewhere) or a
            // damaged entry: the secret is gone, the person signs in again.
            remove(name)
            null
        }
    }

    override fun put(name: String, value: String) {
        val cipher = Cipher.getInstance(TRANSFORMATION)
        cipher.init(Cipher.ENCRYPT_MODE, key())
        val nonce = cipher.iv
        check(nonce.size == NONCE)
        val sealed = nonce + cipher.doFinal(value.toByteArray(Charsets.UTF_8))
        prefs.edit().putString(name, Base64.encodeToString(sealed, Base64.NO_WRAP)).apply()
    }

    override fun remove(name: String) {
        prefs.edit().remove(name).apply()
    }

    companion object {
        const val FILE = "tisma_secrets"
        private const val ALIAS = "tisma_secrets_key"
        private const val TRANSFORMATION = "AES/GCM/NoPadding"
        private const val NONCE = 12

        fun keystoreKey(): SecretKey {
            val ks = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
            (ks.getEntry(ALIAS, null) as? KeyStore.SecretKeyEntry)?.let { return it.secretKey }
            val gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
            gen.init(
                KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256)
                    .build()
            )
            return gen.generateKey()
        }
    }
}
