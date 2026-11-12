package net.tisma.app.engine

/** The engine's face name when [triangles] (normal [n]) of the shown [mesh] lie on the part's end in an axis, or null. */
fun EngineeringCase.Companion.endFace(mesh: MeshFile, triangles: IntArray, n: FloatArray): String? =
    FacePicking.endFace(mesh.positions, mesh.bounds, triangles, n)
