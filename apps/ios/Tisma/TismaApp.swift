import CryptoKit
import SwiftUI
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
    var body: some View {
        TabView {
            PlateView().tabItem { Label("Plate", systemImage: "square.grid.3x3") }
            LibraryView().tabItem { Label("Library", systemImage: "books.vertical") }
            PrinterView().tabItem { Label("Printer", systemImage: "printer") }
            MoreView().tabItem { Label("More", systemImage: "ellipsis.circle") }
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

    private var model: CatalogPrinterModel? { Engine.catalog?.printer(name: printer) }

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
                                Text("The printer's own").tag("")
                                ForEach(c.filamentsFor(p: m), id: \.fullName) { Text($0.fullName).tag($0.fullName) }
                            }
                        }
                    } else {
                        Text("The catalog is missing from this build.").foregroundStyle(.red)
                    }
                }
                Section("Engine") {
                    Text(Engine.run(["--version"]).out.trimmingCharacters(in: .whitespacesAndNewlines))
                    Button("Slice the 20 mm cube") { slice() }.disabled(busy || printer.isEmpty)
                    if busy { ProgressView() }
                    if let result { Text(result).font(.footnote.monospaced()) }
                }
            }
            .navigationTitle("Plate")
        }
    }

    private func slice() {
        busy = true
        let (p, n, f) = (printer, nozzle, filament.isEmpty ? nil : filament)
        Task.detached {
            let dir = FileManager.default.temporaryDirectory
            let model = dir.appendingPathComponent("cube20.stl")
            try? Engine.cubeStl().write(to: model, atomically: true, encoding: .utf8)
            let (config, why) = Engine.profile(printer: p, nozzle: n, filament: f)
            let text: String
            if let config {
                let (r, _, summary) = Engine.slice(model: model, config: config)
                if r.code == 0, let s = summary.flatMap({ EngineBridge.shared.summary(json: $0) }) {
                    text = "\(s.printTime) · \(String(format: "%.2f m", s.filamentMm / 1000)) · \(String(format: "%.1f g", s.filamentG)) · \(s.layers) layers"
                } else {
                    text = "exit \(r.code)\n\(r.err)"
                }
            } else {
                text = "No profile: \(why)"
            }
            await MainActor.run { result = text; busy = false }
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
