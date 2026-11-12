import SwiftUI
import TismaShared

/// The G-code's layers seen from above (mobile_app.md, rule 12; the same parser as Android): one layer at a time with
/// a slider, the layers below in grey.
struct PreviewView: View {
    let gcode: URL
    @State private var layers: GcodeLayers?
    @State private var layer = 0.0
    @State private var failed = false

    var body: some View {
        VStack {
            if let layers, layers.layers.count > 0 {
                Canvas { ctx, size in draw(layers, upTo: Int(layer), in: ctx, size: size) }
                    .background(Color(white: 0.12))
                    .accessibilityIdentifier("preview_canvas")
                Slider(value: $layer, in: 0...Double(layers.layers.count - 1), step: 1)
                    .padding(.horizontal)
                Text("Layer \(Int(layer) + 1) of \(layers.layers.count)").font(.footnote)
            } else if failed {
                ContentUnavailableView("The G-code does not read", systemImage: "exclamationmark.triangle")
            } else {
                ProgressView()
            }
        }
        .navigationTitle("Preview")
        .task {
            let path = gcode.path
            let read = await Task.detached { EngineBridge.shared.preview(path: path) }.value
            layers = read
            failed = read == nil
            layer = Double(max(0, (read?.layers.count ?? 1) - 1))
        }
    }

    private func draw(_ g: GcodeLayers, upTo top: Int, in ctx: GraphicsContext, size: CGSize) {
        let b = g.bounds
        let minX = CGFloat(b.get(index: 0)), minY = CGFloat(b.get(index: 1))
        let w = max(CGFloat(b.get(index: 2)) - minX, 1), h = max(CGFloat(b.get(index: 3)) - minY, 1)
        let scale = min(size.width / w, size.height / h) * 0.9
        let ox = (size.width - w * scale) / 2, oy = (size.height - h * scale) / 2
        func point(_ x: Float, _ y: Float) -> CGPoint {
            CGPoint(x: ox + (CGFloat(x) - minX) * scale, y: size.height - (oy + (CGFloat(y) - minY) * scale))
        }
        let first = max(0, top - 3)
        for i in first...top {
            let l = g.layers[i] as! GcodeLayers.Layer
            let s = l.segments
            var path = Path()
            var k: Int32 = 0
            while k + 3 < s.size {
                path.move(to: point(s.get(index: k), s.get(index: k + 1)))
                path.addLine(to: point(s.get(index: k + 2), s.get(index: k + 3)))
                k += 4
            }
            ctx.stroke(path, with: .color(i == top ? .orange : .gray.opacity(0.5)), lineWidth: i == top ? 1.5 : 1)
        }
    }
}
