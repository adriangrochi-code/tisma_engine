import net.tisma.app.engine.FacePicking
import net.tisma.app.engine.StressMap
import net.tisma.io.BytesInput
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Touching a face and the safety map, shared by Android and the iPhone. */
class FacePickingTest {
    /** A 20 mm cube as a soup, the same corners as the iPhone's test cube (12 triangles, faces looking out). */
    private val cube: FloatArray = run {
        val s = 20f
        val v = listOf(floatArrayOf(0f, 0f, 0f), floatArrayOf(s, 0f, 0f), floatArrayOf(s, s, 0f), floatArrayOf(0f, s, 0f),
            floatArrayOf(0f, 0f, s), floatArrayOf(s, 0f, s), floatArrayOf(s, s, s), floatArrayOf(0f, s, s))
        val faces = listOf(0 to 2 to 1, 0 to 3 to 2, 4 to 5 to 6, 4 to 6 to 7, 0 to 1 to 5, 0 to 5 to 4,
            1 to 2 to 6, 1 to 6 to 5, 2 to 3 to 7, 2 to 7 to 6, 3 to 0 to 4, 3 to 4 to 7)
        faces.flatMap { (ab, c) -> listOf(ab.first, ab.second, c) }.flatMap { v[it].toList() }.toFloatArray()
    }

    @Test
    fun theTopOfTheCubeIsTwoTrianglesAndTheEnginesTopFace() {
        val n = FacePicking.normals(cube)
        val top = FacePicking.flatFace(cube, n, 2)
        assertEquals(setOf(2, 3), top.toSet())
        val normal = FacePicking.normal(cube, top)
        assertEquals(1f, normal[2], 1e-6f)
        assertEquals("top", FacePicking.endFace(cube, FacePicking.bounds(cube), top, normal))
        val box = FacePicking.box(cube, top, 0.08f)
        assertEquals(19.92f, box[2], 1e-4f)
        assertEquals(20.08f, box[5], 1e-4f)
        assertEquals("bottom", FacePicking.endFace(cube, FacePicking.bounds(cube), FacePicking.flatFace(cube, n, 0), floatArrayOf(0f, 0f, -1f)))
    }

    @Test
    fun aSafetyMapQuadIsTwoColoredTriangles() {
        val header = "ply\nformat binary_little_endian 1.0\nelement vertex 4\nproperty float x\nproperty float y\nproperty float z\n" +
            "property uchar red\nproperty uchar green\nproperty uchar blue\nelement face 1\nproperty list uchar int vertex_indices\n" +
            "property float safety\nproperty uchar flag\nend_header\n"
        val body = ArrayList<Byte>()
        fun int(x: Int) { for (k in 0..3) body += (x shr (8 * k)).toByte() }
        for ((x, y) in listOf(0 to 0, 1 to 0, 1 to 1, 0 to 1)) {
            int(x.toFloat().toBits()); int(y.toFloat().toBits()); int(0f.toBits())
            body += 255.toByte(); body += 0; body += 0
        }
        body += 4; int(0); int(1); int(2); int(3); int(1.5f.toBits()); body += 0
        val map = StressMap.parse(BytesInput(header.encodeToByteArray() + body.toByteArray()), floatArrayOf(10f, 0f, 0f))
        assertEquals(2, map.triangles)
        assertEquals(11f, map.positions[3])
        assertTrue(map.colors.withIndex().all { (i, c) -> c == (if (i % 3 == 0) 1f else 0f) })
    }
}
