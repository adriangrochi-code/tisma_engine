import SwiftUI
import TismaShared

/// «Ingeniería» on a bare model (mobile_app.md, rule 13; docs/ANALYSIS.md): a held face, a load pushed into another,
/// the material, the quality and the safety the part must keep; the engine's `analyze` says whether it holds. The
/// iPhone picks the engine's own faces by name (no touch on the 3D part yet).
struct EngineeringView: View {
    /// The model on the plate (nil: the 20 mm cube).
    let model: URL?
    let printer: String
    let nozzle: Double
    let filament: String?

    @State private var fixed = "bottom"
    @State private var loaded = "top"
    @State private var newtons = 100.0
    @State private var material = "bambu-pla-basic"
    @State private var quality = "normal"
    @State private var safety = 2.0
    @State private var quick = false
    @State private var busy = false
    @State private var lines: [ReportLine] = []
    @State private var problem: String?

    private let bridge = EngineeringBridge.shared

    var body: some View {
        Form {
            Section("Part") {
                Text(model?.lastPathComponent ?? String(localized: "The 20 mm cube"))
                Picker("Held face", selection: $fixed) { ForEach(bridge.faces, id: \.self) { Text(Self.face($0)).tag($0) } }
                Picker("Loaded face", selection: $loaded) { ForEach(bridge.faces, id: \.self) { Text(Self.face($0)).tag($0) } }
                Stepper(value: $newtons, in: 1...5000, step: newtons < 100 ? 10 : 50) {
                    LabeledContent("Force", value: String(format: "%.0f N", newtons))
                }
                .accessibilityIdentifier("engineering_force")
            }
            Section("Material and quality") {
                Picker("Material", selection: $material) { ForEach(bridge.materials, id: \.self) { Text($0).tag($0) } }
                Picker("Quality", selection: $quality) {
                    Text("Normal").tag("normal")
                    Text("High").tag("high")
                }
                Stepper(value: $safety, in: 1...10, step: 0.5) {
                    LabeledContent("Safety it must keep", value: String(format: "%.1f", safety))
                }
                Toggle("Quick (the solid part, without slicing)", isOn: $quick)
            }
            Section {
                Button(busy ? String(localized: "Analyzing…") : String(localized: "Analyze")) { analyze() }
                    .disabled(busy || printer.isEmpty || fixed == loaded)
                    .accessibilityIdentifier("engineering_run")
                if printer.isEmpty { Text("Choose a printer on the plate first.").foregroundStyle(.secondary) }
                if fixed == loaded { Text("The held face and the loaded face must differ.").foregroundStyle(.secondary) }
            }
            if !lines.isEmpty {
                Section("Result") {
                    ForEach(Array(lines.enumerated()), id: \.offset) { _, l in
                        let (title, value) = Self.show(l)
                        LabeledContent(title, value: value)
                    }
                }
            }
            if let problem { Section { Text(problem).foregroundStyle(.red) } }
        }
        .navigationTitle("Engineering")
    }

    private func analyze() {
        busy = true; problem = nil; lines = []
        let (p, n, f, chosen) = (printer, nozzle, filament, model)
        let caseJson = bridge.caseJson(fixedFace: fixed, loadFace: loaded, newtons: newtons, material: material,
                                       quality: quality, solid: quick)
        let required = safety
        Task.detached {
            let (found, failed) = Self.run(model: chosen, printer: p, nozzle: n, filament: f, caseJson: caseJson, requiredSafety: required)
            await MainActor.run { lines = found; problem = failed; busy = false }
        }
    }

    /// Runs `analyze` (off the main thread): the result lines, or why there are none.
    nonisolated static func run(model chosen: URL?, printer p: String, nozzle n: Double, filament f: String?, caseJson: String,
                                requiredSafety required: Double) -> ([ReportLine], String?) {
        let dir = FileManager.default.temporaryDirectory
        let part = chosen ?? dir.appendingPathComponent("cube20.stl")
        if chosen == nil { try? Engine.cubeStl().write(to: part, atomically: true, encoding: .utf8) }
        let (config, why) = Engine.profile(printer: p, nozzle: n, filament: f)
        guard let config else { return ([], "No profile: \(why)") }
        let caseFile = dir.appendingPathComponent("case.json")
        let out = dir.appendingPathComponent("report.json")
        try? FileManager.default.removeItem(at: out)
        guard (try? caseJson.write(to: caseFile, atomically: true, encoding: .utf8)) != nil else { return ([], "case.json") }
        let r = Engine.run(EngineeringBridge.shared.analyzeArgs(model: part.path, config: config.path, caseFile: caseFile.path, out: out.path))
        print("ENGINE-ANALYZE exit=\(r.code) err=\(r.err.suffix(300))")
        // 5: no valid result, and the report says why (as on Android).
        if r.code == 0 || r.code == 5, let report = try? String(contentsOf: out, encoding: .utf8) {
            return (EngineeringBridge.shared.report(json: report, requiredSafety: required), nil)
        }
        return ([], r.err.split(separator: "\n").last.map(String.init) ?? "exit \(r.code)")
    }

    static func face(_ name: String) -> String {
        switch name {
        case "top": return String(localized: "Top")
        case "bottom": return String(localized: "Bottom")
        case "x_min": return String(localized: "Left (X−)")
        case "x_max": return String(localized: "Right (X+)")
        case "y_min": return String(localized: "Front (Y−)")
        case "y_max": return String(localized: "Back (Y+)")
        default: return name
        }
    }

    /// A result line in the person's words, as Android's Texts.summary.
    static func show(_ l: ReportLine) -> (String, String) {
        let n = Double(l.value)
        switch l.key {
        case "verdict":
            let v: String
            switch l.value {
            case "holds": v = String(localized: "It holds")
            case "fails": v = String(localized: "It does not hold")
            default: v = String(localized: "No result")
            }
            return (String(localized: "Verdict"), v)
        case "required_safety": return (String(localized: "Safety asked"), n.map { String(format: "%.1f", $0) } ?? l.value)
        case "min_safety": return (String(localized: "Minimum safety"), n.map { String(format: "%.2f", $0) } ?? l.value)
        case "max_displacement_mm": return (String(localized: "Largest displacement"), n.map { String(format: "%.3f mm", $0) } ?? l.value)
        case "max_von_mises_mpa": return (String(localized: "Largest stress"), n.map { String(format: "%.2f MPa", $0) } ?? l.value)
        case "warning": return (String(localized: "Warning"), l.value)
        default: return (l.key, l.value)
        }
    }
}
