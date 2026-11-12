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

/// «Placa»: for now, the engine's first slice on the iPhone (the 20 mm cube); the plate and settings come next.
struct PlateView: View {
    @State private var result: String?
    @State private var busy = false

    var body: some View {
        NavigationStack {
            Form {
                Section("Engine") {
                    Text(Engine.run(["--version"]).out.trimmingCharacters(in: .whitespacesAndNewlines))
                    Button("Slice the 20 mm cube") { slice() }.disabled(busy)
                    if busy { ProgressView() }
                    if let result { Text(result).font(.footnote.monospaced()) }
                }
            }
            .navigationTitle("Plate")
        }
    }

    private func slice() {
        busy = true
        Task.detached {
            let dir = FileManager.default.temporaryDirectory
            let model = dir.appendingPathComponent("cube20.stl")
            try? Engine.cubeStl().write(to: model, atomically: true, encoding: .utf8)
            let (r, gcode, summary) = Engine.slice(model: model)
            let size = (try? FileManager.default.attributesOfItem(atPath: gcode.path)[.size] as? Int) ?? 0
            let text = "exit \(r.code) · G-code \(size) bytes\n" + (summary ?? r.err)
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
