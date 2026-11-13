import SceneKit
import SwiftUI
import TismaShared

/// «Ingeniería» on a bare model (mobile_app.md, rule 13; docs/ANALYSIS.md): a held face and a load pushed into another,
/// touched on the part in 3D (or chosen among the engine's own faces by name), the material, the quality and the safety
/// the part must keep; the engine's `analyze` says whether it holds and colors the part by safety.
struct EngineeringView: View {
    /// The model on the plate (nil: the 20 mm cube).
    let model: URL?
    let printer: String
    let nozzle: Double
    let filament: String?

    /// "touched" for a face touched on the part, else one of the engine's faces.
    @State private var fixed = "bottom"
    @State private var loaded = "top"
    @State private var fixedPick: PickedFace?
    @State private var loadPick: PickedFace?
    /// What a touch on the part marks: "fixed", "load" (the whole face) or "point" (a load at the touched point).
    @State private var marking = "fixed"
    @State private var loadPoint: SCNVector3?
    @State private var direction = "into"
    @State private var temperature = 0.0
    @State private var mesh: PartMesh?
    @State private var newtons = 100.0
    @State private var material = "bambu-pla-basic"
    @State private var quality = "normal"
    @State private var safety = 2.0
    @State private var quick = false
    @State private var busy = false
    @State private var lines: [ReportLine] = []
    @State private var map: StressMapData?
    @State private var showMap = true
    @State private var problem: String?
    @State private var cloudLine: String?
    @State private var cloudLines: [ReportLine] = []
    @State private var lightest: Int?
    @State private var lightestNote: String?

    private let bridge = EngineeringBridge.shared

    private var fixedFace: PickedFace { fixed == "touched" ? (fixedPick ?? bridge.named(face: "bottom")) : bridge.named(face: fixed) }
    private var loadFace: PickedFace { loaded == "touched" ? (loadPick ?? bridge.named(face: "top")) : bridge.named(face: loaded) }
    private var same: Bool {
        fixed != "touched" && loaded != "touched" ? fixed == loaded
            : Set(fixedFace.triangles.map { $0.intValue }) == Set(loadFace.triangles.map { $0.intValue }) && !fixedFace.triangles.isEmpty
    }

    var body: some View {
        Form {
            Section("Part") {
                Text(model?.lastPathComponent ?? String(localized: "The 20 mm cube"))
                if let mesh {
                    PartView(mesh: mesh,
                             held: fixed == "touched" ? (fixedPick?.triangles.map { $0.intValue } ?? []) : [],
                             loaded: loaded == "touched" ? (loadPick?.triangles.map { $0.intValue } ?? []) : [],
                             map: showMap ? map : nil, point: loaded == "touched" ? loadPoint : nil) { touched($0, $1) }
                        .frame(height: 260)
                        .listRowInsets(EdgeInsets())
                    Picker("A touch marks", selection: $marking) {
                        Text("Held face").tag("fixed")
                        Text("Loaded face").tag("load")
                        Text("Point").tag("point")
                    }
                    .pickerStyle(.segmented)
                    if map != nil { Toggle("Safety map", isOn: $showMap) }
                }
                Picker("Held face", selection: $fixed) { faceOptions(touched: fixedPick != nil) }
                Picker("Loaded face", selection: $loaded) { faceOptions(touched: loadPick != nil) }
                Stepper(value: $newtons, in: 1...5000, step: newtons < 100 ? 10 : 50) {
                    LabeledContent("Force", value: String(format: "%.0f N", newtons))
                }
                .accessibilityIdentifier("engineering_force")
                Picker("Direction", selection: $direction) {
                    ForEach(bridge.directions, id: \.self) { Text(Self.direction($0)).tag($0) }
                }
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
                // The nozzle's temperature enters the adhesion between layers (docs/ANALYSIS.md); 0: the filament's.
                Stepper(value: $temperature, in: 0...350, step: 5) {
                    LabeledContent("Nozzle temperature", value: temperature > 0 ? String(format: "%.0f °C", temperature) : String(localized: "The filament's"))
                }
                Toggle("Quick (the solid part, without slicing)", isOn: $quick)
            }
            Section {
                Button(busy ? String(localized: "Analyzing…") : String(localized: "Analyze")) { analyze() }
                    .disabled(busy || printer.isEmpty || same)
                    .accessibilityIdentifier("engineering_run")
                if printer.isEmpty { Text("Choose a printer on the plate first.").foregroundStyle(.secondary) }
                if same { Text("The held face and the loaded face must differ.").foregroundStyle(.secondary) }
            }
            // rule 13: the lightest infill that still holds; the engine finds it on a 3MF project for now.
            Section("Lightest infill") {
                if model?.pathExtension.lowercased() == "3mf" {
                    Button(busy ? String(localized: "Analyzing…") : String(localized: "Find the lightest infill")) { findLightest() }
                        .disabled(busy)
                        .accessibilityIdentifier("engineering_lightest")
                    if let lightest {
                        LabeledContent("Lightest infill that holds", value: "\(lightest) %")
                        Button("Use it for slicing") { useLightest(lightest) }
                    }
                } else {
                    Text("Save it as a project to look for the lightest infill.").foregroundStyle(.secondary)
                }
                if let lightestNote { Text(lightestNote).font(.footnote) }
            }
            // cloud.md, rule 6 (parity P19): the project analyzed in Tisma Cloud, with the conditions it carries.
            if model?.pathExtension.lowercased() == "3mf", Keychain.read("cloud.token") != nil {
                Section("Tisma Cloud") {
                    Button("Analyze in the cloud") { cloud(lightest: false) }.disabled(busy)
                    Button("Lightest infill in the cloud") { cloud(lightest: true) }.disabled(busy)
                    if let cloudLine { Text(cloudLine).font(.footnote) }
                    ForEach(Array(cloudLines.enumerated()), id: \.offset) { _, l in
                        let (title, value) = Self.show(l)
                        LabeledContent(title, value: value)
                    }
                }
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
        .task { await loadMesh() }
    }

    @ViewBuilder private func faceOptions(touched: Bool) -> some View {
        if touched { Text("Touched on the part").tag("touched") }
        ForEach(bridge.faces, id: \.self) { Text(Self.face($0)).tag($0) }
    }

    /// The part to touch: the plate's model (STL or OBJ) or the cube.
    private func loadMesh() async {
        let chosen = model
        let m = await Task.detached { () -> PartMesh? in
            PartMesh.load(chosen ?? Self.cube())
        }.value
        if let m { _ = bridge.setMesh(soup: m.soup) }
        mesh = m
    }

    private func touched(_ triangle: Int, _ at: SCNVector3) {
        if marking == "point" {
            // A point: a box of 1.5 mm around it (Android's point load is a small disc on the surface).
            guard let f = bridge.point(triangle: Int32(triangle), x: Double(at.x), y: Double(at.y), z: Double(at.z), radius: 1.5) else { return }
            loadPick = f; loaded = "touched"; loadPoint = at
        } else {
            guard let f = bridge.pick(triangle: Int32(triangle), quality: quality) else { return }
            if marking == "fixed" { fixedPick = f; fixed = "touched" } else { loadPick = f; loaded = "touched"; loadPoint = nil }
        }
        map = nil; lines = []
    }

    private func cloud(lightest: Bool) {
        guard let project = model, let token = Keychain.read("cloud.token") else { return }
        busy = true; cloudLines = []; cloudLine = String(localized: "Sending…")
        let (server, q, spanish) = (Prefs.server, quality, Locale.current.language.languageCode?.identifier == "es")
        Task.detached {
            do {
                let id = try CloudBridge.shared.submitAnalysis(server: server, token: token, path: project.path,
                                                               name: project.lastPathComponent, lightest: lightest, quality: q)
                while true {
                    let l = try CloudBridge.shared.jobLine(server: server, token: token, id: id, spanish: spanish)
                    await MainActor.run { cloudLine = l.text }
                    if l.finished {
                        let summary = l.done ? ((try? CloudBridge.shared.jobSummary(server: server, token: token, id: id)) ?? []) : []
                        await MainActor.run { cloudLines = summary; busy = false }
                        return
                    }
                    try await Task.sleep(for: .seconds(3))
                }
            } catch {
                let m = error.localizedDescription
                await MainActor.run { cloudLine = m; busy = false }
            }
        }
    }

    private func findLightest() {
        guard let project = model else { return }
        busy = true; lightest = nil; lightestNote = nil
        let q = quality
        Task.detached {
            let dir = FileManager.default.temporaryDirectory
            let out = dir.appendingPathComponent("lightest.tea"), summary = dir.appendingPathComponent("lightest.json")
            try? FileManager.default.removeItem(at: summary)
            let r = Engine.run(EngineeringBridge.shared.lightestArgs(project: project.path, out: out.path, summary: summary.path, quality: q))
            let text = (try? String(contentsOf: summary, encoding: .utf8)) ?? ""
            let d = Int(EngineeringBridge.shared.lightestDensity(summaryJson: text))
            await MainActor.run {
                busy = false
                if d >= 0 { lightest = d } else {
                    lightestNote = r.code == 0 ? String(localized: "No lighter infill holds: keep the one you have.")
                        : (r.err.split(separator: "\n").last.map(String.init) ?? "exit \(r.code)")
                }
            }
        }
    }

    /// The density goes to the person's settings (infill.density), as Android applies it to the slice.
    private func useLightest(_ d: Int) {
        guard let panel = PanelModel.shared.bridge else { return }
        if panel.set(key: "infill.density", text: "\(d)") { PanelModel.shared.changed() }
        lightestNote = String(format: String(localized: "Infill %d %% applied to the settings."), d)
    }

    private func analyze() {
        busy = true; problem = nil; lines = []; map = nil
        let (p, n, f, chosen) = (printer, nozzle, filament, model)
        let caseJson = bridge.caseAlong(fixed: fixedFace, loaded: loadFace, newtons: newtons, direction: direction, material: material,
                                        quality: quality, solid: quick)
        let required = safety
        let celsius = temperature
        Task.detached {
            let (found, failed, voxels) = Self.run(model: chosen, printer: p, nozzle: n, filament: f, caseJson: caseJson,
                                                   requiredSafety: required, nozzleTemperature: celsius)
            let colored = voxels.flatMap { EngineeringBridge.shared.stressMap(path: $0.path, maxFaces: 400_000) }
            await MainActor.run { lines = found; problem = failed; map = colored; showMap = colored != nil; busy = false }
        }
    }

    /// The 20 mm cube as a file, for a plate without a model.
    nonisolated static func cube() -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("cube20.stl")
        if !FileManager.default.fileExists(atPath: url.path) { try? Engine.cubeStl().write(to: url, atomically: true, encoding: .utf8) }
        return url
    }

    /// Runs `analyze` (off the main thread): the result lines or why there are none, and the safety map's file.
    nonisolated static func run(model chosen: URL?, printer p: String, nozzle n: Double, filament f: String?, caseJson: String,
                                requiredSafety required: Double, nozzleTemperature: Double = 0) -> ([ReportLine], String?, URL?) {
        let dir = FileManager.default.temporaryDirectory
        let part = chosen ?? cube()
        let (made, why) = Engine.profile(printer: p, nozzle: n, filament: f)
        guard let profile = made else { return ([], "No profile: \(why)", nil) }
        let config = dir.appendingPathComponent("engineering_config.json")
        let base = (try? String(contentsOf: profile, encoding: .utf8)) ?? "{}"
        try? EngineeringBridge.shared.withNozzleTemperature(configJson: base, celsius: nozzleTemperature).write(to: config, atomically: true, encoding: .utf8)
        let caseFile = dir.appendingPathComponent("case.json")
        let out = dir.appendingPathComponent("report.json")
        let voxels = dir.appendingPathComponent("voxels.ply")
        try? FileManager.default.removeItem(at: out)
        try? FileManager.default.removeItem(at: voxels)
        guard (try? caseJson.write(to: caseFile, atomically: true, encoding: .utf8)) != nil else { return ([], "case.json", nil) }
        let r = Engine.run(EngineeringBridge.shared.analyzeArgs(model: part.path, config: config.path, caseFile: caseFile.path,
                                                                out: out.path, mesh: voxels.path))
        print("ENGINE-ANALYZE exit=\(r.code) err=\(r.err.suffix(300))")
        // 5: no valid result, and the report says why (as on Android).
        if r.code == 0 || r.code == 5, let report = try? String(contentsOf: out, encoding: .utf8) {
            let map = FileManager.default.fileExists(atPath: voxels.path) ? voxels : nil
            return (EngineeringBridge.shared.report(json: report, requiredSafety: required), nil, map)
        }
        return ([], r.err.split(separator: "\n").last.map(String.init) ?? "exit \(r.code)", nil)
    }

    static func direction(_ d: String) -> String {
        switch d {
        case "-z": return String(localized: "Down (−Z)")
        case "+z": return String(localized: "Up (+Z)")
        case "-x": return String(localized: "Left (−X)")
        case "+x": return String(localized: "Right (+X)")
        case "-y": return String(localized: "Toward the front (−Y)")
        case "+y": return String(localized: "Toward the back (+Y)")
        default: return String(localized: "Into the face")
        }
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
