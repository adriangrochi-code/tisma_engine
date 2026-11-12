import SwiftUI
import TismaShared

/// «Calibración» in the Printer tab (calibrations.md; mobile_app.md, rule 8; parity P5): the engine makes the test part
/// and its settings with the height changes for the plate's printer and filament, the app slices it, and after printing
/// the person says which band came out best; that value becomes their setting, as on Android.
struct CalibrationView: View {
    @AppStorage("plate.printer") private var printer = ""
    @AppStorage("plate.nozzle") private var nozzle = 0.4
    @AppStorage("plate.filament") private var filament = ""
    @State private var kind = "temperature"
    @State private var start = ""
    @State private var end = ""
    @State private var step = ""
    @State private var busy = false
    @State private var test: CalTest?
    @State private var gcode: URL?
    @State private var config: URL?
    @State private var best: Double?
    @State private var message: String?
    @State private var problem: String?

    private let bridge = CalibrationBridge.shared

    var body: some View {
        Form {
            Section {
                Picker("Test", selection: $kind) { ForEach(bridge.kinds, id: \.self) { Text(Self.name($0)).tag($0) } }
                if !bridge.byEye(kind: kind) {
                    HStack {
                        TextField("From", text: $start); TextField("To", text: $end); TextField("Step", text: $step)
                    }
                    .keyboardType(.decimalPad)
                }
            } footer: {
                Text(bridge.byEye(kind: kind) ? "The result is read by eye; it sets no value."
                     : "Range: empty for the one the settings suggest. The test uses the plate's printer and filament.")
            }
            Section {
                Button(busy ? String(localized: "Making the test…") : String(localized: "Make the test")) { make() }
                    .disabled(busy || printer.isEmpty)
                    .accessibilityIdentifier("cal_make")
                if printer.isEmpty { Text("Choose a printer on the plate first.").foregroundStyle(.secondary) }
            }
            if let test {
                Section("Bands") {
                    ForEach(Array(test.segments.enumerated()), id: \.offset) { _, s in
                        Text(String(format: "%.1f–%.1f mm: %@ %@", s.z0, s.z1, Self.num(s.value), test.unit))
                    }
                    if let gcode {
                        NavigationLink("Preview") { PreviewView(gcode: gcode) }
                        ShareLink(item: gcode) { Label("Share or save the G-code", systemImage: "square.and.arrow.up") }
                    }
                }
                if !bridge.byEye(kind: kind), !test.segments.isEmpty {
                    Section("Which came out best?") {
                        Picker("Value", selection: $best) {
                            Text("—").tag(Double?.none)
                            ForEach(test.segments.map { $0.value }, id: \.self) { Text("\(Self.num($0)) \(test.unit)").tag(Double?.some($0)) }
                        }
                        Button("Apply to the settings") { apply() }.disabled(best == nil || busy)
                    }
                }
            }
            if let message { Section { Text(message) } }
            if let problem { Section { Text(problem).foregroundStyle(.red) } }
        }
        .navigationTitle("Calibration")
        .onChange(of: kind) { _, _ in test = nil; gcode = nil; best = nil; message = nil }
    }

    private func make() {
        busy = true; problem = nil; message = nil; test = nil; gcode = nil; best = nil
        let range = [start, end, step].map { Double($0.replacingOccurrences(of: ",", with: ".")) ?? 0 }
        let (k, p, n, f) = (kind, printer, nozzle, filament.isEmpty ? nil : filament)
        let panel = PanelModel.shared.bridge
        Task.detached {
            let dir = FileManager.default.temporaryDirectory
            let (profile, why) = Engine.profile(printer: p, nozzle: n, filament: f)
            guard let profile, let base = try? String(contentsOf: profile, encoding: .utf8) else {
                await MainActor.run { problem = "No profile: \(why)"; busy = false }; return
            }
            let config = dir.appendingPathComponent("calibration_config.json")
            try? (panel?.over(profileJson: base) ?? base).write(to: config, atomically: true, encoding: .utf8)
            let model = dir.appendingPathComponent("calibration_\(k).stl")
            let settings = dir.appendingPathComponent("calibration_\(k).json")
            let r = Engine.run(CalibrationBridge.shared.args(kind: k, config: config.path, model: model.path, settings: settings.path,
                                                             start: range[0], end: range[1], step: range[2]))
            print("ENGINE-CAL \(k) exit=\(r.code) out=\(r.out.prefix(200)) err=\(r.err.suffix(200))")
            guard r.code == 0, let t = CalibrationBridge.shared.parse(stdout: r.out) else {
                let m = r.err.split(separator: "\n").last.map(String.init) ?? "exit \(r.code)"
                await MainActor.run { problem = m; busy = false }; return
            }
            // The test part sliced with its own settings (its height changes included).
            let (s, out, _) = Engine.slice(model: model, config: settings)
            await MainActor.run {
                test = t; gcode = s.code == 0 ? out : nil; self.config = config; busy = false
                if s.code != 0 { problem = "exit \(s.code)\n\(s.err)" }
            }
        }
    }

    private func apply() {
        guard let value = best, let config else { return }
        busy = true
        let k = kind
        Task.detached {
            let out = FileManager.default.temporaryDirectory.appendingPathComponent("calibrated.json")
            let r = Engine.run(CalibrationBridge.shared.applyArgs(kind: k, value: value, config: config.path, out: out.path))
            let before = (try? String(contentsOf: config, encoding: .utf8)) ?? "{}"
            let after = (try? String(contentsOf: out, encoding: .utf8)) ?? ""
            await MainActor.run {
                busy = false
                guard r.code == 0, !after.isEmpty, let panel = PanelModel.shared.bridge else {
                    problem = r.err.split(separator: "\n").last.map(String.init) ?? "exit \(r.code)"; return
                }
                _ = panel.adopt(before: before, applied: after)
                PanelModel.shared.changed()
                message = String(format: String(localized: "%@: %@ applied to the settings"), Self.name(k), Self.num(value))
            }
        }
    }

    static func num(_ v: Double) -> String { v == v.rounded() ? String(Int(v)) : String(format: "%g", v) }

    static func name(_ kind: String) -> String {
        switch kind {
        case "first_layer": return String(localized: "First layer")
        case "temperature": return String(localized: "Temperature tower")
        case "flow": return String(localized: "Flow")
        case "retraction": return String(localized: "Retraction")
        case "retraction_speed": return String(localized: "Retraction speed")
        case "pressure_advance": return String(localized: "Pressure advance")
        case "max_flow": return String(localized: "Maximum flow")
        case "speed": return String(localized: "Speed")
        case "acceleration": return String(localized: "Acceleration")
        case "dimensions": return String(localized: "Dimensions")
        case "elephant_foot": return String(localized: "Elephant foot")
        case "bridges_overhangs": return String(localized: "Bridges and overhangs")
        case "fit": return String(localized: "Fit (clearances)")
        default: return kind
        }
    }
}
