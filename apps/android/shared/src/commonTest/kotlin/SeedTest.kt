import net.tisma.app.p2p.LibraryIndex
import net.tisma.app.p2p.PieceStore
import net.tisma.app.p2p.SeedClient
import net.tisma.app.p2p.SeedDownload
import net.tisma.app.p2p.SeedUpload
import net.tisma.cloud.Http
import net.tisma.cloud.HttpRequest
import net.tisma.cloud.HttpResponse
import net.tisma.io.BytesInput
import net.tisma.io.Sha256
import net.tisma.io.hex
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

/** p2p_library.md, rules 8 octies and 8 nonies, against a fake seeder (the same on Android and iOS). */
class SeedTest {
    private val piece = SeedClient.PIECE
    private val data = ByteArray(2 * piece + piece / 2) { (it * 31 + 7).toByte() }
    private val sha = hex(Sha256.digest(data))
    private val hashes = (0 until 3).map { n -> Sha256.digest(data.copyOfRange(n * piece, minOf(data.size, (n + 1) * piece))) }
    private val root = hex(Sha256.Stream().apply { hashes.forEach { update(it) } }.finish())
    private val file = LibraryIndex.FileRef("gear.3mf", "project", data.size.toLong(), sha)

    private class Memory : PieceStore {
        val pieces = mutableMapOf<Int, ByteArray>()
        override fun has(n: Int) = n in pieces
        override fun write(n: Int, bytes: ByteArray) { pieces[n] = bytes }
        override fun read(n: Int) = pieces.getValue(n)
        override fun clear() = pieces.clear()
    }

    /** A seeder: [status] forces an answer, [corrupt] how many times piece 1 comes damaged, [valid] the good ticket. */
    private inner class Seeder(var status: Int = 200, var corrupt: Int = 0, var valid: String = "t1") : Http {
        val seen = mutableListOf<String>()
        val uploaded = mutableMapOf<Int, ByteArray>()
        var have = setOf<Int>()
        var reject: String? = null
        override fun send(request: HttpRequest): HttpResponse {
            val path = request.url.substringAfter("/v1/seed/")
            seen += "${request.method} $path ${request.headers["Authorization"]}"
            fun answer(code: Int, body: ByteArray = ByteArray(0), headers: Map<String, String> = emptyMap()) =
                HttpResponse(code, headers, BytesInput(body))
            if (request.headers["Authorization"] != "Bearer $valid") return answer(401, """{"error":"bad ticket"}""".encodeToByteArray())
            if (status != 200) return answer(status, """{"error":"refused"}""".encodeToByteArray(), mapOf("Retry-After" to "30"))
            return when {
                path == "$sha/pieces" -> answer(200, """{"size":${data.size},"hashes":[${hashes.joinToString(",") { "\"${hex(it)}\"" }}]}""".encodeToByteArray())
                path.startsWith("$sha/piece/") -> {
                    val n = path.substringAfterLast('/').toInt()
                    val bytes = data.copyOfRange(n * piece, minOf(data.size, (n + 1) * piece))
                    if (n == 1 && corrupt > 0) { corrupt--; bytes[0] = (bytes[0] + 1).toByte() }
                    answer(200, bytes)
                }
                path == "upload/$sha" && request.method == "GET" ->
                    answer(200, """{"size":${data.size},"pieces":3,"have":[${(have + uploaded.keys).joinToString(",")}],"complete":${uploaded.size + have.size == 3 && reject == null}}""".encodeToByteArray())
                path.startsWith("upload/$sha/") && request.method == "PUT" -> {
                    val n = path.substringAfterLast('/').toInt()
                    uploaded[n] = request.body ?: ByteArray(0)
                    if (uploaded.size + have.size == 3) {
                        reject?.let { return answer(422, """{"error":"$it"}""".encodeToByteArray()) }
                        answer(201, """{"complete":true}""".encodeToByteArray())
                    } else answer(200, """{"have":[],"pieces":3}""".encodeToByteArray())
                }
                else -> answer(404)
            }
        }
    }

    @Test
    fun aFileComesInCheckedPiecesWithItsTicket() {
        val seeder = Seeder()
        val store = Memory()
        val progress = mutableListOf<Int>()
        val r = SeedDownload(SeedClient("https://cloud.example/", seeder)) { "t1" }.download(file, root, store) { d, _ -> progress += d }
        assertIs<SeedDownload.Result.Done>(r)
        assertTrue((0 until 3).flatMap { store.read(it).toList() }.toByteArray().contentEquals(data))
        assertEquals(listOf(1, 2, 3), progress)
        assertTrue(seeder.seen.all { it.endsWith("Bearer t1") })
        assertEquals("GET $sha/pieces Bearer t1", seeder.seen.first())
    }

    @Test
    fun aListThatDoesNotGiveThePieceRootIsDropped() {
        val r = SeedDownload(SeedClient("https://cloud.example", Seeder())) { "t1" }.download(file, "00".repeat(32), Memory())
        assertIs<SeedDownload.Result.BadList>(r)
    }

    @Test
    fun aDamagedPieceIsAskedAgainAndTwiceDamagedNothingIsKept() {
        val once = Memory()
        assertIs<SeedDownload.Result.Done>(SeedDownload(SeedClient("https://c", Seeder(corrupt = 1))) { "t1" }.download(file, root, once))
        val twice = Memory()
        assertIs<SeedDownload.Result.Corrupt>(SeedDownload(SeedClient("https://c", Seeder(corrupt = 2))) { "t1" }.download(file, root, twice))
        assertTrue(twice.pieces.isEmpty())
    }

    @Test
    fun theSeedersAnswersBecomeTheAppsNotices() {
        fun get(status: Int) = SeedDownload(SeedClient("https://c", Seeder(status = status))) { "t1" }.download(file, root, Memory())
        assertIs<SeedDownload.Result.NotYet>(get(404))
        assertIs<SeedDownload.Result.Blocked>(get(451))
        assertIs<SeedDownload.Result.Blocked>(get(403))
        assertEquals(30L, assertIs<SeedDownload.Result.Busy>(get(429)).retryAfterS)
    }

    @Test
    fun anExpiredTicketIsRenewedOnce() {
        val tickets = ArrayDeque(listOf("old", "t1"))
        val r = SeedDownload(SeedClient("https://c", Seeder())) { tickets.removeFirst() }.download(file, root, Memory())
        assertIs<SeedDownload.Result.Done>(r)
        assertTrue(tickets.isEmpty())
        val never = SeedDownload(SeedClient("https://c", Seeder(valid = "x"))) { "t1" }.download(file, root, Memory())
        assertIs<SeedDownload.Result.Unauthorized>(never)
    }

    @Test
    fun aResumedDownloadAsksOnlyForTheMissingPieces() {
        val seeder = Seeder()
        val store = Memory().apply { write(0, data.copyOfRange(0, piece)) }
        assertIs<SeedDownload.Result.Done>(SeedDownload(SeedClient("https://c", seeder)) { "t1" }.download(file, root, store))
        assertEquals(listOf("$sha/pieces", "$sha/piece/1", "$sha/piece/2"), seeder.seen.map { it.split(' ')[1] })
    }

    @Test
    fun anUploadResumesFromWhatTheSeederHas() {
        val seeder = Seeder().apply { have = setOf(0) }
        fun slice(n: Int) = data.copyOfRange(n * piece, minOf(data.size, (n + 1) * piece))
        val r = SeedUpload(SeedClient("https://c", seeder)) { "t1" }.upload(sha, data.size.toLong(), ::slice)
        assertIs<SeedUpload.Result.Done>(r)
        assertEquals(setOf(1, 2), seeder.uploaded.keys)
        assertTrue(seeder.uploaded.getValue(2).contentEquals(slice(2)))
        val refused = Seeder().apply { reject = "not a 3MF" }
        val no = SeedUpload(SeedClient("https://c", refused)) { "t1" }.upload(sha, data.size.toLong(), ::slice)
        assertEquals("not a 3MF", assertIs<SeedUpload.Result.Rejected>(no).reason)
    }

    @Test
    fun theStreamedHashIsTheWholeOne() {
        for (n in listOf(0, 1, 55, 56, 63, 64, 65, 1000)) {
            val b = ByteArray(n) { (it * 7).toByte() }
            val s = Sha256.Stream()
            b.toList().chunked(13).forEach { s.update(it.toByteArray()) }
            assertTrue(s.finish().contentEquals(Sha256.digest(b)), "length $n")
        }
        // FIPS 180-4 example: "abc".
        assertEquals("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", hex(Sha256.digest("abc".encodeToByteArray())))
    }
}
