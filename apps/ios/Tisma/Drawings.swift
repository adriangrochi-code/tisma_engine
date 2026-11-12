import SwiftUI
import TismaShared

/// One-ink drawings in the accent colour on its soft background (interface.md, «Aspecto»), in both themes; the same
/// shapes as Android's (settings_panel.md, rules 34 to 36).
private struct Plate: View {
    let size: CGFloat
    let draw: (inout GraphicsContext, CGSize, Color) -> Void

    var body: some View {
        Canvas { ctx, s in draw(&ctx, s, .accentColor) }
            .frame(width: size, height: size)
            .background(Color.accentColor.opacity(0.15), in: RoundedRectangle(cornerRadius: 8))
    }
}

private func line(_ ctx: inout GraphicsContext, _ ink: Color, _ a: CGPoint, _ b: CGPoint, _ width: CGFloat) {
    var p = Path(); p.move(to: a); p.addLine(to: b)
    ctx.stroke(p, with: .color(ink), lineWidth: width)
}

/// The printer by its kind: open, enclosed, delta, belt, resin or pellets.
struct PrinterDrawing: View {
    let shape: String
    var size: CGFloat = 48

    var body: some View {
        Plate(size: size) { ctx, s, ink in
            let w = s.width, h = s.height, sw = w * 0.05
            func pt(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: w * x, y: h * y) }
            func rect(_ x: CGFloat, _ y: CGFloat, _ rw: CGFloat, _ rh: CGFloat) -> CGRect { CGRect(x: w * x, y: h * y, width: w * rw, height: h * rh) }
            switch shape {
            case "enclosed":
                ctx.stroke(Path(roundedRect: rect(0.15, 0.15, 0.7, 0.7), cornerRadius: w * 0.06), with: .color(ink), lineWidth: sw)
                ctx.fill(Path(rect(0.44, 0.25, 0.12, 0.14)), with: .color(ink))
                line(&ctx, ink, pt(0.25, 0.7), pt(0.75, 0.7), sw)
            case "delta":
                line(&ctx, ink, pt(0.25, 0.15), pt(0.25, 0.85), sw)
                line(&ctx, ink, pt(0.75, 0.15), pt(0.75, 0.85), sw)
                line(&ctx, ink, pt(0.25, 0.3), pt(0.5, 0.5), sw * 0.6)
                line(&ctx, ink, pt(0.75, 0.3), pt(0.5, 0.5), sw * 0.6)
                ctx.stroke(Path(ellipseIn: rect(0.3, 0.74, 0.4, 0.1)), with: .color(ink), lineWidth: sw)
            case "belt":
                ctx.stroke(Path(roundedRect: rect(0.1, 0.68, 0.8, 0.14), cornerRadius: h * 0.07), with: .color(ink), lineWidth: sw)
                line(&ctx, ink, pt(0.3, 0.2), pt(0.6, 0.62), sw)
                ctx.fill(Path(rect(0.55, 0.5, 0.1, 0.12)), with: .color(ink))
            case "resin":
                ctx.stroke(Path(rect(0.2, 0.55, 0.6, 0.3)), with: .color(ink), lineWidth: sw)
                ctx.stroke(Path(roundedRect: rect(0.3, 0.15, 0.4, 0.4), cornerRadius: w * 0.05), with: .color(ink), lineWidth: sw)
                line(&ctx, ink, pt(0.5, 0.1), pt(0.5, 0.3), sw)
            case "pellets":
                var hopper = Path()
                hopper.move(to: pt(0.3, 0.15)); hopper.addLine(to: pt(0.7, 0.15)); hopper.addLine(to: pt(0.55, 0.45))
                hopper.addLine(to: pt(0.45, 0.45)); hopper.closeSubpath()
                ctx.stroke(hopper, with: .color(ink), lineWidth: sw)
                ctx.fill(Path(rect(0.44, 0.45, 0.12, 0.2)), with: .color(ink))
                line(&ctx, ink, pt(0.15, 0.8), pt(0.85, 0.8), sw * 1.4)
            default: // open: two uprights, the gantry across, a nozzle under it, the bed below
                line(&ctx, ink, pt(0.2, 0.2), pt(0.2, 0.8), sw)
                line(&ctx, ink, pt(0.8, 0.2), pt(0.8, 0.8), sw)
                line(&ctx, ink, pt(0.2, 0.3), pt(0.8, 0.3), sw)
                ctx.fill(Path(rect(0.44, 0.3, 0.12, 0.14)), with: .color(ink))
                line(&ctx, ink, pt(0.15, 0.8), pt(0.85, 0.8), sw * 1.4)
            }
        }
        .accessibilityIdentifier("drawing_printer_" + shape)
    }
}

/// The bed by its plate's family: textured, smooth, glass, cool, engineering or other.
struct BedDrawing: View {
    let family: String
    var size: CGFloat = 32

    var body: some View {
        Plate(size: size) { ctx, s, ink in
            let w = s.width, h = s.height
            ctx.stroke(Path(roundedRect: CGRect(x: w * 0.12, y: h * 0.35, width: w * 0.76, height: h * 0.3), cornerRadius: w * 0.04),
                       with: .color(ink), lineWidth: w * 0.05)
            switch family {
            case "textured":
                for i in 0...4 { for j in 0...1 {
                    let c = CGPoint(x: w * (0.22 + CGFloat(i) * 0.14), y: h * (0.44 + CGFloat(j) * 0.12)), r = w * 0.025
                    ctx.fill(Path(ellipseIn: CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r)), with: .color(ink))
                } }
            case "smooth": line(&ctx, ink.opacity(0.5), CGPoint(x: w * 0.2, y: h * 0.45), CGPoint(x: w * 0.5, y: h * 0.45), w * 0.04)
            case "glass": line(&ctx, ink, CGPoint(x: w * 0.3, y: h * 0.6), CGPoint(x: w * 0.5, y: h * 0.4), w * 0.04)
            case "cool":
                let c = CGPoint(x: w * 0.5, y: h * 0.5), r = w * 0.1
                for a in 0..<3 {
                    let t = Double(a) * .pi / 3
                    line(&ctx, ink, CGPoint(x: c.x - r * cos(t), y: c.y - r * sin(t)), CGPoint(x: c.x + r * cos(t), y: c.y + r * sin(t)), w * 0.03)
                }
            case "engineering":
                var flame = Path()
                flame.move(to: CGPoint(x: w * 0.5, y: h * 0.38))
                flame.addQuadCurve(to: CGPoint(x: w * 0.5, y: h * 0.62), control: CGPoint(x: w * 0.62, y: h * 0.52))
                flame.addQuadCurve(to: CGPoint(x: w * 0.5, y: h * 0.38), control: CGPoint(x: w * 0.38, y: h * 0.52))
                ctx.fill(flame, with: .color(ink))
            default: break
            }
        }
        .accessibilityIdentifier("drawing_bed_" + family)
    }
}

/// The nozzle: brass filled, hardened steel striped; high flow longer; the tip grows from 0.2 to 1.2 mm.
struct NozzleDrawing: View {
    let diameter: Double
    let hardened: Bool
    let highFlow: Bool
    var size: CGFloat = 32

    var body: some View {
        Plate(size: size) { ctx, s, ink in
            let w = s.width, h = s.height
            let bottom = highFlow ? h * 0.62 : h * 0.5
            let tip = w * (0.06 + CGFloat(min(max(diameter, 0.2), 1.2) - 0.2) * 0.14)
            var body = Path()
            body.move(to: CGPoint(x: w * 0.32, y: h * 0.15)); body.addLine(to: CGPoint(x: w * 0.68, y: h * 0.15))
            body.addLine(to: CGPoint(x: w * 0.68, y: bottom)); body.addLine(to: CGPoint(x: w * 0.5 + tip, y: h * 0.85))
            body.addLine(to: CGPoint(x: w * 0.5 - tip, y: h * 0.85)); body.addLine(to: CGPoint(x: w * 0.32, y: bottom))
            body.closeSubpath()
            if hardened {
                ctx.stroke(body, with: .color(ink), lineWidth: w * 0.05)
                for i in 1...3 {
                    line(&ctx, ink, CGPoint(x: w * 0.34, y: h * (0.15 + CGFloat(i) * 0.1)), CGPoint(x: w * 0.66, y: h * (0.08 + CGFloat(i) * 0.1)), w * 0.04)
                }
            } else {
                ctx.fill(body, with: .color(ink))
            }
        }
        .accessibilityIdentifier("drawing_nozzle" + (hardened ? "_hardened" : "") + (highFlow ? "_hf" : ""))
    }
}

/// The connection icon's states (settings_panel.md, rule 42), as on Android.
enum Connection: String {
    case none, connecting, connected, error

    /// From the saved printer and the watcher's last reading.
    static func of(address: String, line: PrinterLine?, problem: String?) -> Connection {
        if address.isEmpty { return .none }
        if let line { return line.state == "offline" || line.state == "error" ? .error : .connected }
        return problem != nil ? .error : .connecting
    }

    var text: LocalizedStringKey {
        switch self {
        case .none: return "No printer connected"
        case .connecting: return "Connecting to the printer"
        case .connected: return "Printer connected"
        case .error: return "The printer does not answer"
        }
    }

    var tint: Color {
        switch self {
        case .none: return .secondary
        case .connecting: return .accentColor
        case .connected: return .green
        case .error: return .red
        }
    }
}

struct ConnectionIcon: View {
    let connection: Connection
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Image(systemName: connection == .error ? "wifi.exclamationmark" : connection == .none ? "wifi.slash" : "wifi")
                .foregroundStyle(connection.tint)
                .font(.title3)
        }
        .buttonStyle(.borderless)
        .accessibilityLabel(Text(connection.text))
        .accessibilityIdentifier("connection_" + connection.rawValue)
    }
}

/// The plate's printer card (mobile_app.md, rule 6): the printer's drawing and name, the nozzle, the filament, the
/// plate of a Bambu Lab, and the connection icon (a tap opens the Printer tab).
struct PrinterCard: View {
    let printer: String
    let nozzle: Double
    let filament: String
    let look: PrinterCardLook?
    let connection: Connection
    let onConnection: () -> Void

    var body: some View {
        HStack(spacing: 12) {
            if printer.isEmpty {
                Image(systemName: "printer").font(.title2).frame(width: 48, height: 48)
            } else {
                PrinterDrawing(shape: look?.shape ?? "open")
            }
            VStack(alignment: .leading, spacing: 4) {
                Text(printer.isEmpty ? String(localized: "No printer chosen") : printer).font(.headline)
                if !printer.isEmpty {
                    HStack(spacing: 6) {
                        NozzleDrawing(diameter: nozzle, hardened: look?.hardened ?? false, highFlow: look?.highFlow ?? false, size: 24)
                        Text(String(format: "%.2f mm", nozzle) + ((look?.hardened ?? false) ? " · " + String(localized: "hardened steel") : ""))
                            .font(.caption)
                    }
                }
                Text(printer.isEmpty ? String(localized: "Choose it in the assistant or below.") : filament)
                    .font(.caption).foregroundStyle(.secondary)
            }
            Spacer(minLength: 0)
            if let look, look.bambu {
                VStack {
                    BedDrawing(family: look.bed)
                    Text(look.plate.isEmpty ? String(localized: "auto") : look.plate).font(.caption2)
                        .accessibilityIdentifier("printer_card_plate")
                }
            }
            ConnectionIcon(connection: connection, action: onConnection)
        }
        .accessibilityElement(children: .contain)
        .accessibilityIdentifier("printer_card")
    }
}
