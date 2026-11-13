import ModelIO
import SceneKit
import SwiftUI
import TismaShared

/// A part as a triangle soup: float x y z per corner, three corners per triangle, in the model file's coordinates
/// (the engine's, so a touched face goes to it unchanged).
struct PartMesh {
    let soup: Data
    var triangles: Int { soup.count / 36 }

    /// min x, y, z, max x, y, z.
    var bounds: [Float] {
        var b: [Float] = [.greatestFiniteMagnitude, .greatestFiniteMagnitude, .greatestFiniteMagnitude,
                          -.greatestFiniteMagnitude, -.greatestFiniteMagnitude, -.greatestFiniteMagnitude]
        soup.withUnsafeBytes { raw in
            let f = raw.bindMemory(to: Float.self)
            for i in stride(from: 0, to: f.count - 2, by: 3) {
                for a in 0..<3 { b[a] = min(b[a], f[i + a]); b[a + 3] = max(b[a + 3], f[i + a]) }
            }
        }
        return soup.isEmpty ? [0, 0, 0, 1, 1, 1] : b
    }

    /// Reads an STL or OBJ with ModelIO (Apple's own reader); nil for what it cannot read (a 3MF: the faces by name).
    static func load(_ url: URL) -> PartMesh? {
        let asset = MDLAsset(url: url)
        var out = Data()
        for case let mesh as MDLMesh in asset.childObjects(of: MDLMesh.self) {
            guard let attr = mesh.vertexAttributeData(forAttributeNamed: MDLVertexAttributePosition, as: .float3) else { continue }
            let base = attr.dataStart
            let stride = attr.stride
            func corner(_ i: Int) {
                let p = base.advanced(by: i * stride).assumingMemoryBound(to: Float.self)
                withUnsafeBytes(of: (p[0], p[1], p[2])) { out.append(contentsOf: $0) }
            }
            for case let sub as MDLSubmesh in mesh.submeshes ?? [] where sub.geometryType == .triangles {
                let map = sub.indexBuffer.map()
                let n = sub.indexCount
                switch sub.indexType {
                case .uInt32: let ix = map.bytes.assumingMemoryBound(to: UInt32.self); for k in 0..<n { corner(Int(ix[k])) }
                case .uInt16: let ix = map.bytes.assumingMemoryBound(to: UInt16.self); for k in 0..<n { corner(Int(ix[k])) }
                case .uInt8: let ix = map.bytes.assumingMemoryBound(to: UInt8.self); for k in 0..<n { corner(Int(ix[k])) }
                default: break
                }
            }
        }
        return out.isEmpty ? nil : PartMesh(soup: out)
    }
}

/// The part in 3D (mobile_app.md, rule 13): turned with a finger, and a tap gives the touched triangle. Marked faces are
/// drawn over it (blue held, orange loaded); after an analysis the safety map can replace it.
struct PartView: UIViewRepresentable {
    let mesh: PartMesh
    var held: [Int] = []
    var loaded: [Int] = []
    var map: StressMapData?
    /// On the plate: the bed (min x, min y, max x, max y) with the part standing on its middle.
    var bed: [Double]?
    /// A change re-frames the camera («Encuadrar»).
    var frame = 0
    /// Where the camera looks from (rule 6, the View tools): 3d, top, front, left, right or back.
    var viewFrom = "3d"
    /// A point load's place (the model file's coordinates), drawn as a small ball.
    var point: SCNVector3?
    /// The touched triangle and the touched point on it (the model file's coordinates).
    var onTap: (Int, SCNVector3) -> Void = { _, _ in }

    func makeCoordinator() -> Coordinator { Coordinator(onTap: onTap) }

    func makeUIView(context: Context) -> SCNView {
        let view = SCNView()
        view.scene = SCNScene()
        view.allowsCameraControl = true
        view.autoenablesDefaultLighting = true
        view.backgroundColor = .secondarySystemBackground
        view.addGestureRecognizer(UITapGestureRecognizer(target: context.coordinator, action: #selector(Coordinator.tapped(_:))))
        view.accessibilityIdentifier = "engineering_part"
        return view
    }

    func updateUIView(_ view: SCNView, context: Context) {
        context.coordinator.onTap = onTap
        let root = view.scene!.rootNode
        root.childNodes.filter { $0.name != "camera" }.forEach { $0.removeFromParentNode() }
        defer {
            // The first time and on «Encuadrar»: the camera in front of and above the whole scene.
            if context.coordinator.framed != frame || context.coordinator.viewFrom != viewFrom {
                context.coordinator.framed = frame
                context.coordinator.viewFrom = viewFrom
                Self.frame(view, root, from: viewFrom)
            }
        }
        // The engine's Z is up; SceneKit's Y.
        let part = SCNNode()
        part.eulerAngles.x = -.pi / 2
        root.addChildNode(part)
        if let bed, bed.count == 4 {
            let w = CGFloat(bed[2] - bed[0]), h = CGFloat(bed[3] - bed[1])
            let plate = SCNNode(geometry: SCNBox(width: w, height: h, length: 1, chamferRadius: 0))
            plate.geometry?.firstMaterial?.diffuse.contents = UIColor.tertiarySystemFill
            plate.position = SCNVector3(Float(bed[0]) + Float(w) / 2, Float(bed[1]) + Float(h) / 2, -0.5)
            part.addChildNode(plate)
            // The part on the bed's middle, standing on it (as Android's plate).
            let b = mesh.bounds
            let holder = SCNNode()
            holder.position = SCNVector3(Float(bed[0] + bed[2]) / 2 - (b[0] + b[3]) / 2, Float(bed[1] + bed[3]) / 2 - (b[1] + b[4]) / 2, -b[2])
            part.addChildNode(holder)
            let body = SCNNode(geometry: Self.geometry(mesh.soup, color: .systemTeal))
            holder.addChildNode(body)
            return
        }
        if let map {
            part.addChildNode(SCNNode(geometry: Self.geometry(map.positions, colors: map.colors)))
        } else {
            let body = SCNNode(geometry: Self.geometry(mesh.soup, color: .systemGray4))
            body.name = "part"
            part.addChildNode(body)
            if !held.isEmpty { part.addChildNode(SCNNode(geometry: Self.geometry(Self.subset(mesh.soup, held), color: .systemBlue, lift: 0.05))) }
            if !loaded.isEmpty && point == nil { part.addChildNode(SCNNode(geometry: Self.geometry(Self.subset(mesh.soup, loaded), color: .systemOrange, lift: 0.05))) }
            if let point {
                let b = mesh.bounds
                let ball = SCNNode(geometry: SCNSphere(radius: CGFloat(max(b[3] - b[0], b[4] - b[1], b[5] - b[2]) * 0.03)))
                ball.geometry?.firstMaterial?.diffuse.contents = UIColor.systemOrange
                ball.position = point
                part.addChildNode(ball)
            }
        }
    }

    /// Puts the camera where the whole scene shows.
    static func frame(_ view: SCNView, _ root: SCNNode, from: String = "3d") {
        let content = root.childNodes.filter { $0.name != "camera" }
        guard !content.isEmpty else { return }
        var lo = SCNVector3(Float.greatestFiniteMagnitude, .greatestFiniteMagnitude, .greatestFiniteMagnitude)
        var hi = SCNVector3(-Float.greatestFiniteMagnitude, -.greatestFiniteMagnitude, -.greatestFiniteMagnitude)
        for n in content {
            let (a, b) = n.boundingBox
            for p in [n.convertPosition(a, to: root), n.convertPosition(b, to: root)] {
                lo = SCNVector3(min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)); hi = SCNVector3(max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z))
            }
        }
        let c = SCNVector3((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2)
        let r = max(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, 1)
        let camera = root.childNode(withName: "camera", recursively: false) ?? {
            let n = SCNNode(); n.name = "camera"; n.camera = SCNCamera(); n.camera?.zFar = 100_000; root.addChildNode(n); return n
        }()
        // The engine's Z is SceneKit's Y, and the engine's front (−Y) is SceneKit's +Z.
        let d = r * 1.9
        switch from {
        case "top": camera.position = SCNVector3(c.x, c.y + d, c.z); camera.look(at: c, up: SCNVector3(0, 0, -1), localFront: SCNVector3(0, 0, -1))
        case "front": camera.position = SCNVector3(c.x, c.y, c.z + d); camera.look(at: c)
        case "back": camera.position = SCNVector3(c.x, c.y, c.z - d); camera.look(at: c)
        case "left": camera.position = SCNVector3(c.x - d, c.y, c.z); camera.look(at: c)
        case "right": camera.position = SCNVector3(c.x + d, c.y, c.z); camera.look(at: c)
        default: camera.position = SCNVector3(c.x, c.y + r * 0.9, c.z + r * 1.6); camera.look(at: c)
        }
        view.pointOfView = camera
    }

    final class Coordinator: NSObject {
        var framed = -1
        var viewFrom = "3d"
        var onTap: (Int, SCNVector3) -> Void
        init(onTap: @escaping (Int, SCNVector3) -> Void) { self.onTap = onTap }

        @objc func tapped(_ g: UITapGestureRecognizer) {
            guard let view = g.view as? SCNView else { return }
            let hits = view.hitTest(g.location(in: view), options: [.searchMode: SCNHitTestSearchMode.closest.rawValue])
            if let hit = hits.first(where: { $0.node.name == "part" }) { onTap(hit.faceIndex, hit.localCoordinates) }
        }
    }

    /// The corners of [triangles] out of [soup].
    static func subset(_ soup: Data, _ triangles: [Int]) -> Data {
        var out = Data(capacity: triangles.count * 36)
        for t in triangles where (t + 1) * 36 <= soup.count { out.append(soup.subdata(in: t * 36..<(t + 1) * 36)) }
        return out
    }

    /// A flat-shaded geometry from a soup; [lift] mm along each triangle's normal (marks drawn over the part).
    static func geometry(_ soup: Data, color: UIColor? = nil, colors: Data? = nil, lift: Float = 0) -> SCNGeometry {
        let n = soup.count / 12
        var points = [SCNVector3](repeating: SCNVector3Zero, count: n)
        var normals = [SCNVector3](repeating: SCNVector3Zero, count: n)
        soup.withUnsafeBytes { raw in
            let f = raw.bindMemory(to: Float.self)
            for t in 0..<(n / 3) {
                let o = 9 * t
                let ux = f[o + 3] - f[o], uy = f[o + 4] - f[o + 1], uz = f[o + 5] - f[o + 2]
                let vx = f[o + 6] - f[o], vy = f[o + 7] - f[o + 1], vz = f[o + 8] - f[o + 2]
                var nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx
                let l = (nx * nx + ny * ny + nz * nz).squareRoot()
                if l > 0 { nx /= l; ny /= l; nz /= l }
                for k in 0..<3 {
                    let i = o + 3 * k
                    points[3 * t + k] = SCNVector3(f[i] + nx * lift, f[i + 1] + ny * lift, f[i + 2] + nz * lift)
                    normals[3 * t + k] = SCNVector3(nx, ny, nz)
                }
            }
        }
        var sources = [SCNGeometrySource(vertices: points), SCNGeometrySource(normals: normals)]
        if let colors {
            sources.append(SCNGeometrySource(data: colors, semantic: .color, vectorCount: n, usesFloatComponents: true,
                                             componentsPerVector: 3, bytesPerComponent: 4, dataOffset: 0, dataStride: 12))
        }
        let indices = (0..<Int32(n)).map { $0 }
        let element = SCNGeometryElement(indices: indices, primitiveType: .triangles)
        let g = SCNGeometry(sources: sources, elements: [element])
        let m = SCNMaterial()
        m.diffuse.contents = colors == nil ? (color ?? .systemGray4) : UIColor.white
        m.isDoubleSided = true
        g.materials = [m]
        return g
    }
}
