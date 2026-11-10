package net.tisma.app

import net.tisma.app.engine.MeshFile
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.test.assertEquals

/** Robolectric: the 3MF reader uses Android's XML pull parser. */
@org.junit.runner.RunWith(org.robolectric.RobolectricTestRunner::class)
@org.robolectric.annotation.Config(sdk = [35])
class MeshFileTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private val root = File(System.getProperty("user.dir")).parentFile.parentFile.parentFile

    @Test
    fun `ASCII and binary STL`() {
        val ascii = MeshFile.read(File(root, "tests/data/cube_ascii.stl"))
        assertEquals(12, ascii.triangles)
        val bin = tmp.newFile("t.stl")
        val bb = ByteBuffer.allocate(84 + 50).order(ByteOrder.LITTLE_ENDIAN)
        bb.position(80); bb.putInt(1)
        bb.position(84 + 12)
        listOf(0f, 0f, 0f, 10f, 0f, 0f, 0f, 10f, 5f).forEach { bb.putFloat(it) }
        bin.writeBytes(bb.array())
        val m = MeshFile.read(bin)
        assertEquals(1, m.triangles)
        assertEquals(listOf(0f, 0f, 0f, 10f, 10f, 5f), m.bounds.toList())
    }

    @Test
    fun `OBJ quads become two triangles`() {
        val f = tmp.newFile("q.obj").apply { writeText("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n") }
        assertEquals(2, MeshFile.read(f).triangles)
    }

    @Test
    fun `OBJ and ASCII STL with bad lines skip them instead of closing the app`() {
        val obj = tmp.newFile("bad.obj").apply {
            writeText("v\t0 0 0\nv 1 0 0\nv 0 1 0\nv 1 2\nvn 0 0 1\nf 1 2 3\nf 1 2 9\nf 0 1 2\nf -3 -2 -1\nf 1 2\n")
        }
        assertEquals(2, MeshFile.read(obj).triangles)
        val stl = tmp.newFile("bad.stl").apply {
            writeText("solid x\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 nan-ish\nvertex 0 1 0\nendloop\nendfacet\n" +
                "facet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid x\n")
        }
        assertEquals(1, MeshFile.read(stl).triangles)
    }

    @Test
    fun `3MF build items, components and transforms`() {
        val model = """<model unit="millimeter"><resources>
            <object id="1" type="model"><mesh><vertices><vertex x="0" y="0" z="0"/><vertex x="1" y="0" z="0"/><vertex x="0" y="1" z="0"/></vertices>
            <triangles><triangle v1="0" v2="1" v3="2"/></triangles></mesh></object>
            <object id="2" type="model"><components><component objectid="1" transform="1 0 0 0 1 0 0 0 1 10 0 0"/></components></object>
            </resources><build><item objectid="2" transform="2 0 0 0 2 0 0 0 2 0 0 5"/></build></model>"""
        val f = tmp.newFile("a.3mf")
        java.util.zip.ZipOutputStream(f.outputStream()).use { z ->
            z.putNextEntry(java.util.zip.ZipEntry("3D/3dmodel.model")); z.write(model.toByteArray())
        }
        val m = MeshFile.read(f)
        assertEquals(1, m.triangles)
        // (component +10 in x) then (item scale 2, +5 in z): x from 20 to 22, z 5.
        assertEquals(listOf(20f, 0f, 5f, 22f, 2f, 5f), m.bounds.toList())
        val onBed = m.onBed(100f, 100f)
        assertEquals(listOf(99f, 99f, 0f, 101f, 101f, 0f), onBed.bounds.toList())
    }

    @Test
    fun `a 3MF project with objects in other model files, as slicer projects save them, and bad indices skipped`() {
        val root = """<model unit="millimeter" xmlns:p="http://schemas.microsoft.com/3dmanufacturing/production/2015/06"><resources>
            <object id="5" type="model"><components><component p:path="/3D/Objects/object_1.model" objectid="1" transform="1 0 0 0 1 0 0 0 1 10 0 0"/></components></object>
            </resources><build><item objectid="5"/></build></model>"""
        val part = """<model unit="millimeter"><resources>
            <object id="1" type="model"><mesh><vertices><vertex x="0" y="0" z="0"/><vertex x="1" y="0" z="0"/><vertex x="0" y="1" z="0"/></vertices>
            <triangles><triangle v1="0" v2="1" v3="2"/><triangle v1="0" v2="1" v3="99"/></triangles></mesh></object></resources></model>"""
        val f = tmp.newFile("project.3mf")
        java.util.zip.ZipOutputStream(f.outputStream()).use { z ->
            // The part first: the root model is found by its name, not by its place in the ZIP.
            z.putNextEntry(java.util.zip.ZipEntry("3D/Objects/object_1.model")); z.write(part.toByteArray())
            z.putNextEntry(java.util.zip.ZipEntry("3D/3dmodel.model")); z.write(root.toByteArray())
        }
        val m = MeshFile.read(f)
        assertEquals(1, m.triangles)
        assertEquals(listOf(10f, 0f, 0f, 11f, 1f, 0f), m.bounds.toList())
    }

    @Test
    fun `the orbit camera frames a box and its views look along the axes`() {
        val cam = net.tisma.app.ui.OrbitCamera()
        cam.frame(floatArrayOf(90f, 90f, 0f, 110f, 110f, 20f))
        assertEquals(listOf(100.0, 100.0, 10.0), cam.target.toList())
        cam.set(net.tisma.app.ui.OrbitCamera.View.TOP)
        val e = cam.eye()
        kotlin.test.assertTrue(e[2] > 10 + cam.distance * 0.99)
        cam.set(net.tisma.app.ui.OrbitCamera.View.FRONT)
        kotlin.test.assertTrue(cam.eye()[1] < 100 - cam.distance * 0.99) // from the front (−Y)
        // The target projects to the center of the view.
        val m = cam.viewProjection(1f)
        val t = cam.target
        val x = m[0] * t[0] + m[4] * t[1] + m[8] * t[2] + m[12]
        val y = m[1] * t[0] + m[5] * t[1] + m[9] * t[2] + m[13]
        val w = m[3] * t[0] + m[7] * t[1] + m[11] * t[2] + m[15]
        assertEquals(0.0, x / w, 1e-6)
        assertEquals(0.0, y / w, 1e-6)
    }
}
