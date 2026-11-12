import CryptoKit
import SwiftUI
import UniformTypeIdentifiers
import TismaShared

/// Tisma for iPhone: the same sections as the Android app (mobile_app.md, rule 5). Slicing waits for the engine as a
/// library (apps/ios/PLAN.md, step 4); until then the plate says so.
@main
struct TismaApp: App {
    init() {
        // Signatures (the shared library's key, list and index) are checked by the system: CryptoKit's Ed25519.
        LibraryBridge.shared.useEd25519 { key, message, signature in
            let ok = (try? Curve25519.Signing.PublicKey(rawRepresentation: key as Data))?
                .isValidSignature(signature as Data, for: message as Data) ?? false
            return KotlinBoolean(bool: ok)
        }
    }

    var body: some Scene {
        WindowGroup { RootView() }
    }
}

struct RootView: View {
    @State private var tab = 0
    @AppStorage("plate.opened") private var openedPath = ""

    var body: some View {
        TabView(selection: $tab) {
            PlateView().tabItem { Label("Plate", systemImage: "square.grid.3x3") }.tag(0)
            LibraryView().tabItem { Label("Library", systemImage: "books.vertical") }.tag(1)
            PrinterView().tabItem { Label("Printer", systemImage: "printer") }.tag(2)
            MoreView().tabItem { Label("More", systemImage: "ellipsis.circle") }.tag(3)
        }
        // «Compartir con Tisma» from another app (MA5): the model goes onto the plate.
        .onOpenURL { url in
            if let copy = PlateView.copyIn(url) { openedPath = copy.path; tab = 0 }
        }
    }
}

/// «Placa»: the printer and filament from the engine's catalog, and a slice of the 20 mm cube with their profile
/// (mobile_app.md, rules 11 and 12). Opening the person's models and the preview come next.
struct PlateView: View {
    @AppStorage("plate.printer") private var printer = ""
    @AppStorage("plate.nozzle") private var nozzle = 0.4
    @AppStorage("plate.filament") private var filament = ""
    @State private var result: String?
    @State private var busy = false
    @State private var gcode: URL?
    /// The model on the plate: the person's (copied into the app) or, until one is opened, the 20 mm cube.
    @AppStorage("plate.opened") private var openedPath = ""
    private var opened: URL? { openedPath.isEmpty ? nil : URL(fileURLWithPath: openedPath) }
    @State private var picking = false
    @State private var sent: String?
    @State private var cloudLine: String?
    // Print and Material (mobile_app.md, rule 7): the usual changes over the printer's profile.
    @AppStorage("print.layer") private var layerHeight = 0.2
    @AppStorage("print.infill") private var infill = 20.0
    @AppStorage("print.supports") private var supports = false
    @AppStorage("material.nozzle") private var nozzleTemp = 0.0
    @AppStorage("material.bed") private var bedTemp = 0.0
    // The printer saved on the Printer screen.
    @AppStorage("printer.kind") private var savedKind = "moonraker"
    @AppStorage("printer.address") private var savedAddress = ""
    @AppStorage("printer.user") private var savedUser = ""

    private var model: Catalog.PrinterModel? { Engine.catalog?.printer(name: printer) }

    var body: some View {
        NavigationStack {
            Form {
                Section("Printer") {
                    if let c = Engine.catalog {
                        Picker("Printer", selection: $printer) {
                            Text("Choose…").tag("")
                            ForEach(c.printers, id: \.name) { Text($0.name).tag($0.name) }
                        }
                        if let m = model {
                            Picker("Nozzle", selection: $nozzle) {
                                ForEach(m.nozzles.map { $0.doubleValue }, id: \.self) { Text(String(format: "%.2f mm", $0)).tag($0) }
                            }
                            Picker("Filament", selection: $filament) {
                                ForEach(c.filamentsFor(p: m), id: \.fullName) { Text($0.fullName).tag($0.fullName) }
                            }
                        }
                    } else {
                        Text("The catalog is missing from this build.").foregroundStyle(.red)
                    }
                }
                Section("Print") {
                    Stepper(value: $layerHeight, in: 0.05...0.4, step: 0.02) {
                        LabeledContent("Layer height", value: String(format: "%.2f mm", layerHeight))
                    }
                    VStack(alignment: .leading) {
                        LabeledContent("Infill", value: "\(Int(infill)) %")
                        Slider(value: $infill, in: 0...100, step: 5)
                    }
                    Toggle("Supports", isOn: $supports)
                }
                Section("Material") {
                    Stepper(value: $nozzleTemp, in: 0...320, step: 5) {
                        LabeledContent("Nozzle", value: nozzleTemp > 0 ? "\(Int(nozzleTemp)) °C" : String(localized: "The filament's"))
                    }
                    Stepper(value: $bedTemp, in: 0...120, step: 5) {
                        LabeledContent("Bed", value: bedTemp > 0 ? "\(Int(bedTemp)) °C" : String(localized: "The filament's"))
                    }
                }
                Section("Model") {
                    Text(opened?.lastPathComponent ?? String(localized: "The 20 mm cube"))
                    Button("Open a model…") { picking = true }
                }
                // cloud.md, rules 6 to 9 (MA10): a 3MF project sliced in the cloud, followed, and its G-code previewed.
                if let o = opened, o.pathExtension.lowercased() == "3mf", Keychain.read("cloud.token") != nil {
                    Section("Tisma Cloud") {
                        Button("Slice in the cloud") { cloudSlice(o) }.disabled(busy)
                        if let cloudLine { Text(cloudLine).font(.footnote) }
                    }
                }
                Section("Engine") {
                    Text(Engine.run(["--version"]).out.trimmingCharacters(in: .whitespacesAndNewlines))
                    Button("Slice") { slice() }.disabled(busy || printer.isEmpty)
                    if busy { ProgressView() }
                    if let result { Text(result).font(.footnote.monospaced()) }
                    if let gcode {
                        NavigationLink("Preview") { PreviewView(gcode: gcode) }
                        Button("Send to the printer and start") { send(gcode) }.disabled(busy || savedAddress.isEmpty)
                        if let sent { Text(sent).font(.footnote) }
                    }
                }
            }
            .navigationTitle("Plate")
            // mobile_app.md, rule 10: STL, OBJ and 3MF from the phone's files (STEP waits for the engine).
            .fileImporter(isPresented: $picking, allowedContentTypes: Self.modelTypes) { r in
                if case .success(let url) = r { openedPath = Self.copyIn(url)?.path ?? ""; gcode = nil; result = nil }
            }
        }
    }

    static let modelTypes: [UTType] = ["stl", "obj", "3mf"].compactMap { UTType(filenameExtension: $0) } + [.data]

    /// A picked file is copied into the app (the picker's access ends); nil when it cannot be read or is no model.
    static func copyIn(_ url: URL) -> URL? {
        guard ["stl", "obj", "3mf"].contains(url.pathExtension.lowercased()) else { return nil }
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("models", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        let dest = dir.appendingPathComponent(url.lastPathComponent)
        try? FileManager.default.removeItem(at: dest)
        return (try? FileManager.default.copyItem(at: url, to: dest)) != nil ? dest : nil
    }

    private func cloudSlice(_ project: URL) {
        busy = true; cloudLine = nil
        let server = CloudBridge.shared.defaultServer
        let token = Keychain.read("cloud.token") ?? ""
        let spanish = Locale.current.language.languageCode?.identifier == "es"
        Task.detached {
            do {
                let id = try CloudBridge.shared.submitSlice(server: server, token: token, path: project.path, name: project.lastPathComponent)
                // Followed every 2 s (rule 8) until it ends.
                while true {
                    let line = try CloudBridge.shared.jobLine(server: server, token: token, id: id, spanish: spanish)
                    await MainActor.run { cloudLine = line.text }
                    if line.finished {
                        if line.done {
                            let dest = FileManager.default.temporaryDirectory.appendingPathComponent("cloud-\(id).gcode")
                            try CloudBridge.shared.downloadResult(server: server, token: token, id: id, dest: dest.path)
                            await MainActor.run { gcode = dest }
                        }
                        break
                    }
                    try await Task.sleep(for: .seconds(2))
                }
            } catch {
                let m = error.localizedDescription
                await MainActor.run { cloudLine = m }
            }
            await MainActor.run { busy = false }
        }
    }

    /// mobile_app.md, rule 14 (MA7): the G-code goes to the saved printer, which starts it.
    private func send(_ file: URL) {
        busy = true; sent = nil
        let a = (savedKind, savedAddress, savedUser, Keychain.read("printer.secret") ?? "")
        let name = (opened?.deletingPathExtension().lastPathComponent ?? "cube20") + ".gcode"
        Task.detached {
            let text: String
            do {
                try PrinterBridge.shared.upload(kind: a.0, address: a.1, user: a.2, secret: a.3, path: file.path, name: name, start: true)
                text = String(localized: "Sent; the printer starts it.")
            } catch {
                text = error.localizedDescription
            }
            await MainActor.run { sent = text; busy = false }
        }
    }

    private func slice() {
        busy = true
        // A filament gives the temperatures: without one the engine refuses the G-code (nozzle heater at 0).
        let fallback = Engine.catalog.flatMap { c in model.flatMap { c.filamentsFor(p: $0).first?.fullName } }
        let (p, n, f, chosen) = (printer, nozzle, filament.isEmpty ? fallback : filament, opened)
        let changes = (layerHeight, infill, supports, nozzleTemp, bedTemp)
        Task.detached {
            let dir = FileManager.default.temporaryDirectory
            let model = chosen ?? dir.appendingPathComponent("cube20.stl")
            if chosen == nil { try? Engine.cubeStl().write(to: model, atomically: true, encoding: .utf8) }
            let (config, why) = Engine.profile(printer: p, nozzle: n, filament: f)
            let text: String
            var made: URL?
            if let profile = config {
                let config = profile.deletingPathExtension().appendingPathExtension("changed.json")
                let applied = EngineBridge.shared.applyChanges(profilePath: profile.path, outPath: config.path,
                    layerHeight: changes.0, infillPercent: changes.1, supports: changes.2, nozzleTemp: changes.3, bedTemp: changes.4)
                let (r, out, summary) = Engine.slice(model: model, config: applied ? config : profile)
                if r.code == 0 { made = out }
                if r.code == 0, let s = summary.flatMap({ EngineBridge.shared.summary(json: $0) }) {
                    text = "\(s.printTime) · \(String(format: "%.2f m", s.filamentMm / 1000)) · \(String(format: "%.1f g", s.filamentG)) · \(s.layers) layers"
                } else {
                    text = "exit \(r.code)\n\(r.err)"
                }
            } else {
                text = "No profile: \(why)"
            }
            await MainActor.run { result = text; gcode = made; busy = false }
        }
    }
}

/// «Biblioteca compartida» (p2p_library.md): the signed index, searched like on Android (accents ignored).
struct LibraryView: View {
    /// The project's Ed25519 key (as on Android, BuildConfig.UPDATES_KEY): empty until the owner's key exists, and
    /// then nothing signed is accepted.
    static let projectKey = ""

    @State private var query = ""
    @State private var items: [SharedModel] = []
    @State private var problem: String?

    var body: some View {
        NavigationStack {
            List(items, id: \.id) { m in
                VStack(alignment: .leading) {
                    Text(m.name).font(.headline)
                    Text("\(m.author) · \(m.license)").font(.subheadline).foregroundStyle(.secondary)
                }
            }
            .overlay {
                if items.isEmpty {
                    ContentUnavailableView("No models yet", systemImage: "books.vertical",
                                           description: problem.map { Text($0) })
                }
            }
            .searchable(text: $query)
            .onChange(of: query) { _, q in items = LibraryBridge.shared.search(query: q) }
            .task { await refresh() }
            .refreshable { await refresh() }
            .navigationTitle("Library")
        }
    }

    private func refresh() async {
        let server = CloudBridge.shared.defaultServer
        let why = await Task.detached { LibraryBridge.shared.refresh(server: server, projectKeyHex: LibraryView.projectKey) }.value
        problem = why
        items = LibraryBridge.shared.search(query: query)
    }
}
