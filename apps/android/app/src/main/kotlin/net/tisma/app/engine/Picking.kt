package net.tisma.app.engine

import kotlin.math.abs

/**
 * Touching the model in the 3D view (mobile_app.md, rule 13): the ray under
 * the finger, the triangle it meets first (Möller & Trumbore 1997, «Fast,
 * minimum storage ray/triangle intersection»), and the flat face around it:
 * the triangles joined to it that lie in its plane. A face is given to the
 * engine as a box around it (docs/ANALYSIS.md, «El caso»). Own design.
 */
object Picking {
    /** The ray through a point of the view, x and y from -1 to 1 (y up), for the camera's view-projection [vp]. */
    fun ray(vp: FloatArray, x: Float, y: Float): Pair<FloatArray, FloatArray> {
        val inv = FloatArray(16)
        android.opengl.Matrix.invertM(inv, 0, vp, 0)
        fun unproject(z: Float): FloatArray {
            val v = floatArrayOf(x, y, z, 1f)
            val r = FloatArray(4)
            android.opengl.Matrix.multiplyMV(r, 0, inv, 0, v, 0)
            return floatArrayOf(r[0] / r[3], r[1] / r[3], r[2] / r[3])
        }
        val near = unproject(-1f)
        val far = unproject(1f)
        return near to floatArrayOf(far[0] - near[0], far[1] - near[1], far[2] - near[2])
    }

    /** The first triangle the ray meets, or null. */
    fun hit(mesh: MeshFile, o: FloatArray, d: FloatArray): Int? {
        val p = mesh.positions
        var best = Float.MAX_VALUE
        var found: Int? = null
        for (t in 0 until mesh.triangles) {
            val i = 9 * t
            val e1x = p[i + 3] - p[i]; val e1y = p[i + 4] - p[i + 1]; val e1z = p[i + 5] - p[i + 2]
            val e2x = p[i + 6] - p[i]; val e2y = p[i + 7] - p[i + 1]; val e2z = p[i + 8] - p[i + 2]
            val px = d[1] * e2z - d[2] * e2y; val py = d[2] * e2x - d[0] * e2z; val pz = d[0] * e2y - d[1] * e2x
            val det = e1x * px + e1y * py + e1z * pz
            if (abs(det) < 1e-12f) continue
            val inv = 1f / det
            val tx = o[0] - p[i]; val ty = o[1] - p[i + 1]; val tz = o[2] - p[i + 2]
            val u = (tx * px + ty * py + tz * pz) * inv
            if (u < 0f || u > 1f) continue
            val qx = ty * e1z - tz * e1y; val qy = tz * e1x - tx * e1z; val qz = tx * e1y - ty * e1x
            val v = (d[0] * qx + d[1] * qy + d[2] * qz) * inv
            if (v < 0f || u + v > 1f) continue
            val dist = (e2x * qx + e2y * qy + e2z * qz) * inv
            if (dist > 0f && dist < best) { best = dist; found = t }
        }
        return found
    }

    /** The flat face of [seed] (the shared [FacePicking], also the iPhone's). */
    fun flatFace(mesh: MeshFile, seed: Int, maxAngleDeg: Double = 5.0, tolerance: Float = 0.05f): IntArray =
        FacePicking.flatFace(mesh.positions, mesh.normals, seed, maxAngleDeg, tolerance)

    /** The box around [triangles] (min x y z, max x y z), [pad] mm larger on every side. */
    fun box(mesh: MeshFile, triangles: IntArray, pad: Float): FloatArray = FacePicking.box(mesh.positions, triangles, pad)

    /** The area-weighted normal of [triangles], unit length. */
    fun normal(mesh: MeshFile, triangles: IntArray): FloatArray = FacePicking.normal(mesh.positions, triangles)
}
