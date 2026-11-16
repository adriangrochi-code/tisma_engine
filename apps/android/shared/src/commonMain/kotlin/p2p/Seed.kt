package net.tisma.app.p2p

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.longOrNull
import net.tisma.cloud.Http
import net.tisma.cloud.HttpRequest
import net.tisma.cloud.readUpTo
import net.tisma.io.IOException
import net.tisma.io.Sha256
import net.tisma.io.hex

/**
 * Tisma Cloud's seeder over HTTPS (p2p_library.md, rules 8 octies and 8 nonies, provisional until the apps speak the
 * peer network): the piece list and the pieces of a library file, and the upload of a newly published one. Served at
 * the master's public address under `/v1/seed/`, with a ticket as `Authorization: Bearer`. Own design over the spec;
 * the same on Android and iOS.
 */
class SeedClient(serverUrl: String, private val http: Http) {
    private val base = serverUrl.trimEnd('/') + "/v1/seed/"

    /** A refusal of the seeder: its HTTP status and, for 429, the seconds it asks to wait. */
    class Refused(val status: Int, val retryAfterS: Long?, val reason: String) : Exception("seeder $status $reason")

    /** Rule 8 septies' `pieces` answer: the file's size and the SHA-256 of each 1 MiB piece. */
    class PieceList(val size: Long, val hashes: List<ByteArray>)

    /** Rule 8 nonies' upload state: what the seeder already holds, to resume. */
    class UploadState(val size: Long, val pieces: Int, val have: Set<Int>, val complete: Boolean)

    fun pieces(sha256: String, ticket: String): PieceList {
        val o = json(call("GET", "$sha256/pieces", ticket, maxBytes = 1 shl 20))
        val hashes = (o["hashes"] as? JsonArray).orEmpty().map { (it as? JsonPrimitive)?.contentOrNull?.let(::unhex) ?: ByteArray(0) }
        return PieceList((o["size"] as? JsonPrimitive)?.longOrNull ?: -1, hashes)
    }

    fun piece(sha256: String, n: Int, ticket: String): ByteArray = call("GET", "$sha256/piece/$n", ticket, maxBytes = PIECE + 1)

    fun uploadState(sha256: String, ticket: String): UploadState {
        val o = json(call("GET", "upload/$sha256", ticket, maxBytes = 1 shl 20))
        return UploadState((o["size"] as? JsonPrimitive)?.longOrNull ?: -1, (o["pieces"] as? JsonPrimitive)?.intOrNull ?: -1,
            have(o), (o["complete"] as? JsonPrimitive)?.booleanOrNull ?: false)
    }

    /** Puts piece [n]; true when it was the last and the seeder took the whole file (201). */
    fun putPiece(sha256: String, n: Int, bytes: ByteArray, ticket: String): Boolean {
        var created = false
        call("PUT", "upload/$sha256/$n", ticket, body = bytes, maxBytes = 1 shl 20) { created = it == 201 }
        return created
    }

    private fun call(method: String, path: String, ticket: String, body: ByteArray? = null, maxBytes: Int,
                     status: (Int) -> Unit = {}): ByteArray {
        val headers = buildMap {
            put("Authorization", "Bearer $ticket")
            if (body != null) put("Content-Type", "application/octet-stream")
        }
        val r = http.send(HttpRequest(method, base + path, headers, body, timeoutMs = 120_000))
        r.use {
            if (!r.ok) {
                val text = runCatching { r.text(4096) }.getOrDefault("")
                val reason = runCatching { (Json.parseToJsonElement(text) as? JsonObject)?.get("error")?.let { (it as? JsonPrimitive)?.contentOrNull } }
                    .getOrNull() ?: text.take(200)
                val wait = r.headers.entries.firstOrNull { it.key.equals("Retry-After", ignoreCase = true) }?.value?.trim()?.toLongOrNull()
                throw Refused(r.status, wait, reason)
            }
            status(r.status)
            val b = r.body.readUpTo(maxBytes + 1)
            if (b.size > maxBytes) throw IOException("The seeder's answer is larger than $maxBytes bytes")
            return b
        }
    }

    private fun json(b: ByteArray): JsonObject =
        runCatching { Json.parseToJsonElement(b.decodeToString()) as JsonObject }.getOrNull() ?: throw IOException("The seeder's answer cannot be read")

    private fun have(o: JsonObject): Set<Int> = (o["have"] as? JsonArray).orEmpty().mapNotNull { (it as? JsonPrimitive)?.intOrNull }.toSet()

    companion object {
        /** Rule 3 ter, `swarm.piece_size`: 1 MiB, the last piece shorter. */
        const val PIECE = 1 shl 20

        fun pieceCount(size: Long): Int = if (size <= 0) 0 else ((size + PIECE - 1) / PIECE).toInt()

        fun unhex(h: String): ByteArray {
            val t = h.trim()
            if (t.length % 2 != 0 || t.any { it !in "0123456789abcdefABCDEF" }) return ByteArray(0)
            return ByteArray(t.length / 2) { t.substring(2 * it, 2 * it + 2).toInt(16).toByte() }
        }
    }
}

/** Where a download keeps its pieces (a file on the phone), so it can resume and be checked whole at the end. */
interface PieceStore {
    fun has(n: Int): Boolean
    fun write(n: Int, bytes: ByteArray)
    fun read(n: Int): ByteArray
    /** Drops what was kept (a file that did not check out). */
    fun clear()
}

/**
 * Rule 8 octies: a library file from the seeder, checked as from a peer. The piece list must give the index's
 * `piece_roots` (else it is dropped, P2P10), each piece its hash in the list (a bad one is asked once more), and the
 * whole file its SHA-256; a 401 asks for a new ticket once.
 */
class SeedDownload(private val client: SeedClient, private val ticket: () -> String) {
    sealed class Result {
        object Done : Result()
        /** 404: «Este modelo todavía no está disponible para bajar». */
        object NotYet : Result()
        /** 403 or 451: the account, the peer or the file is blocked. */
        object Blocked : Result()
        /** 429: past the hour's cap; try again after [retryAfterS]. */
        class Busy(val retryAfterS: Long?) : Result()
        object Unauthorized : Result()
        /** The piece list does not give `piece_roots`, or the size differs from the index. */
        object BadList : Result()
        /** A piece or the whole file did not match its hash: nothing of it is kept. */
        object Corrupt : Result()
    }

    fun download(file: LibraryIndex.FileRef, pieceRoot: String, store: PieceStore, progress: (done: Int, of: Int) -> Unit = { _, _ -> }): Result {
        var t = ticket()
        fun <T> withTicket(block: (String) -> T): T = try {
            block(t)
        } catch (e: SeedClient.Refused) {
            if (e.status != 401) throw e
            t = ticket()
            block(t)
        }
        return try {
            val list = withTicket { client.pieces(file.sha256, it) }
            val root = Sha256.Stream().apply { list.hashes.forEach { update(it) } }.finish()
            val count = SeedClient.pieceCount(list.size)
            if (hex(root) != pieceRoot.lowercase() || list.size != file.size || list.hashes.size != count ||
                list.hashes.any { it.size != 32 }) return Result.BadList
            for (n in 0 until count) {
                if (!store.has(n)) {
                    val want = if (n == count - 1) (list.size - n.toLong() * SeedClient.PIECE).toInt() else SeedClient.PIECE
                    var bytes = withTicket { client.piece(file.sha256, n, it) }
                    if (bytes.size != want || !Sha256.digest(bytes).contentEquals(list.hashes[n])) {
                        bytes = withTicket { client.piece(file.sha256, n, it) }
                        if (bytes.size != want || !Sha256.digest(bytes).contentEquals(list.hashes[n])) { store.clear(); return Result.Corrupt }
                    }
                    store.write(n, bytes)
                }
                progress(n + 1, count)
            }
            val whole = Sha256.Stream().apply { for (n in 0 until count) update(store.read(n)) }.finish()
            if (hex(whole) != file.sha256.lowercase()) { store.clear(); Result.Corrupt } else Result.Done
        } catch (e: SeedClient.Refused) {
            when (e.status) {
                404 -> Result.NotYet
                403, 451 -> Result.Blocked
                429 -> Result.Busy(e.retryAfterS)
                401 -> Result.Unauthorized
                else -> throw IOException("seeder ${e.status} ${e.reason}")
            }
        }
    }
}

/**
 * Rule 8 nonies: a published file to the seeder in 1 MiB pieces, resumed from what it already has. [ticket] gives the
 * upload ticket (from publishing, or new ones from `POST /v1/library/models/<id>/uploads` when they expire).
 */
class SeedUpload(private val client: SeedClient, private val ticket: () -> String) {
    sealed class Result {
        /** The seeder holds the whole file (201, or it already had it) and serves it from now on. */
        object Done : Result()
        /** 422: the format or antivirus check refused it, with [reason]; the seeder deleted it. */
        class Rejected(val reason: String) : Result()
        class Busy(val retryAfterS: Long?) : Result()
        /** 507: the seeder has no room; the owner is told. */
        object NoRoom : Result()
        object Blocked : Result()
        object Unauthorized : Result()
        /** 409 or 413: a piece or the size does not match what was published. */
        class Mismatch(val reason: String) : Result()
    }

    fun upload(sha256: String, size: Long, piece: (Int) -> ByteArray, progress: (done: Int, of: Int) -> Unit = { _, _ -> }): Result {
        var t = ticket()
        fun <T> withTicket(block: (String) -> T): T = try {
            block(t)
        } catch (e: SeedClient.Refused) {
            if (e.status != 401) throw e
            t = ticket()
            block(t)
        }
        return try {
            val state = withTicket { client.uploadState(sha256, it) }
            if (state.complete) return Result.Done
            val count = SeedClient.pieceCount(size)
            var complete = false
            var sent = state.have.size
            for (n in 0 until count) {
                if (n in state.have) continue
                complete = withTicket { client.putPiece(sha256, n, piece(n), it) } || complete
                progress(++sent, count)
            }
            // The seeder answers 201 to the piece that completes it; one already complete before says so on GET.
            if (complete || withTicket { client.uploadState(sha256, it) }.complete) Result.Done
            else Result.Mismatch("the seeder did not take the whole file")
        } catch (e: SeedClient.Refused) {
            when (e.status) {
                422 -> Result.Rejected(e.reason)
                429 -> Result.Busy(e.retryAfterS)
                507 -> Result.NoRoom
                403, 451 -> Result.Blocked
                401 -> Result.Unauthorized
                409, 413 -> Result.Mismatch(e.reason)
                else -> throw IOException("seeder ${e.status} ${e.reason}")
            }
        }
    }
}
