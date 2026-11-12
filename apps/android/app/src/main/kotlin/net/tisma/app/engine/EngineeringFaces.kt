package net.tisma.app.engine

/** The engine's face name when [triangles] (normal [n]) of the shown [mesh] lie on the part's end in an axis, or null. */
fun EngineeringCase.Companion.endFace(mesh: MeshFile, triangles: IntArray, n: FloatArray): String? {
    val axis = (0..2).firstOrNull { kotlin.math.abs(n[it]) > 0.996f } ?: return null
    val b = mesh.bounds
    val max = n[axis] > 0
    val target = if (max) b[axis + 3] else b[axis]
    for (t in triangles) for (k in 0..2) if (kotlin.math.abs(mesh.positions[9 * t + 3 * k + axis] - target) > 0.01f) return null
    return when (axis) {
        0 -> if (max) "x_max" else "x_min"
        1 -> if (max) "y_max" else "y_min"
        else -> if (max) "top" else "bottom"
    }
}
