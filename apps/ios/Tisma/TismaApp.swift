import CryptoKit
import SwiftUI
import UserNotifications
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
        // UI tests start as on a new phone (a launch argument would shadow the app's own write of wizard.done).
        if ProcessInfo.processInfo.arguments.contains("--first-start") {
            UserDefaults.standard.removeObject(forKey: "wizard.done")
        }
        // The print notices (docs/MOBILE.md, «Avisos»): shown while the app is open too, and read in the background.
        UNUserNotificationCenter.current().delegate = NoticeDelegate.shared
        PrintWatcher.registerBackground()
    }

    var body: some Scene {
        WindowGroup { RootView() }
    }
}

struct RootView: View {
    @State private var tab = 0
    @AppStorage("plate.opened") private var openedPath = ""
    @AppStorage("wizard.done") private var wizardDone = false
    @Environment(\.scenePhase) private var phase

    var body: some View {
        TabView(selection: $tab) {
            // mobile_app.md, rule 5: Plate, Print, Material, Printer and More (the library is in More, rule 9).
            PlateView(openPrinter: { tab = 3 }).tabItem { Label("Plate", systemImage: "square.grid.3x3") }.tag(0)
            SettingsView(tab: "print", title: "Print").tabItem { Label("Print", systemImage: "slider.horizontal.3") }.tag(1)
            SettingsView(tab: "material", title: "Material").tabItem { Label("Material", systemImage: "drop") }.tag(2)
            PrinterView().tabItem { Label("Printer", systemImage: "printer") }.tag(3)
            MoreView().tabItem { Label("More", systemImage: "ellipsis.circle") }.tag(4)
        }
        // The first-use assistant (MA1), once.
        .fullScreenCover(isPresented: Binding(get: { !wizardDone }, set: { _ in })) { WizardView() }
        // «Compartir con Tisma» from another app (MA5): the model goes onto the plate.
        .onOpenURL { url in
            if let copy = PlateView.copyIn(url) { openedPath = copy.path; tab = 0 }
        }
        // The saved printer is followed while the app is open; in the background, only while it prints.
        .onChange(of: phase, initial: true) { _, now in
            if now == .active { PrintWatcher.shared.start() } else {
                PrintWatcher.shared.stop()
                if now == .background && PrintWatcher.shared.printing { PrintWatcher.scheduleBackground() }
            }
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
    // The printer saved on the Printer screen.
    @AppStorage("printer.kind") private var savedKind = "moonraker"
    @AppStorage("printer.address") private var savedAddress = ""
    @AppStorage("printer.user") private var savedUser = ""
    /// The connection icon opens the Printer tab.
    var openPrinter: () -> Void = {}
    @ObservedObject private var watcher = PrintWatcher.shared
    @State private var look: PrinterCardLook?
    @State private var partMesh: PartMesh?
    @State private var job: Engine.SliceJob?
    @State private var percent = 0
    @State private var stageText = ""
    @State private var framing = 0

    private var model: Catalog.PrinterModel? { Engine.catalog?.printer(name: printer) }

    var body: some View {
        NavigationStack {
            Form {
                // rule 6: the bed and the model in 3D (one finger turns, two move, pinch zooms; «Frame» puts it back).
                if let partMesh {
                    Section {
                        PartView(mesh: partMesh, bed: look?.bedArea.map { $0.doubleValue } ?? [0, 0, 220, 220], frame: framing)
                            .frame(height: 280)
                            .listRowInsets(EdgeInsets())
                            .accessibilityIdentifier("plate_3d")
                        Button("Frame") { framing += 1 }
                    }
                }
                Section {
                    PrinterCard(printer: printer, nozzle: nozzle, filament: filament, look: look,
                                connection: Connection.of(address: savedAddress, line: watcher.line, problem: watcher.problem),
                                onConnection: openPrinter)
                        // The drawings come from the printer's profile, made by the engine when the choice changes.
                        .task(id: "\(printer)|\(nozzle)|\(filament)") { await loadLook() }
                }
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
                Section("Model") {
                    Text(opened?.lastPathComponent ?? String(localized: "The 20 mm cube"))
                    Button("Open a model…") { picking = true }
                    // mobile_app.md, rule 13: whether the part holds, with the engine's analysis.
                    NavigationLink("Engineering") {
                        EngineeringView(model: opened, printer: printer, nozzle: nozzle, filament: filament.isEmpty
                            ? Engine.catalog.flatMap { c in model.flatMap { c.filamentsFor(p: $0).first?.fullName } } : filament)
                    }
                    .accessibilityIdentifier("plate_engineering")
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
                    // rule 12: the slice in the background, with its progress and a way to stop it.
                    if busy {
                        ProgressView(value: Double(percent), total: 100) { Text(stageText.isEmpty ? String(localized: "Slicing…") : String(localized: String.LocalizationValue(stageText))) }
                        if let job { Button("Cancel", role: .cancel) { job.cancel() }.accessibilityIdentifier("plate_cancel") }
                    }
                    if let result { Text(result).font(.footnote.monospaced()) }
                    if let gcode {
                        NavigationLink("Preview") { PreviewView(gcode: gcode) }
                        // rule 6: the output, saved or shared (Files, AirDrop, another app).
                        ShareLink(item: gcode) { Label("Share or save the G-code", systemImage: "square.and.arrow.up") }
                            .accessibilityIdentifier("plate_share")
                        Button("Send to the printer and start") { send(gcode) }.disabled(busy || savedAddress.isEmpty)
                        if let sent { Text(sent).font(.footnote) }
                    }
                }
            }
            .navigationTitle("Plate")
            .task(id: openedPath) {
                let chosen = opened
                partMesh = await Task.detached { PartMesh.load(chosen ?? EngineeringView.cube()) }.value
                framing += 1
            }
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
        let server = Prefs.server
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

    /// The card's look from `tisma-engine profile` for the chosen printer (off the main thread).
    private func loadLook() async {
        guard !printer.isEmpty else { look = nil; return }
        let (p, n, f) = (printer, nozzle, filament.isEmpty ? nil : filament)
        look = await Task.detached { () -> PrinterCardLook? in
            let (url, _) = Engine.profile(printer: p, nozzle: n, filament: f)
            guard let url, let json = try? String(contentsOf: url, encoding: .utf8) else { return nil }
            try? FileManager.default.removeItem(at: url)
            return EngineBridge.shared.cardLook(profileJson: json, name: p)
        }.value
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

    /// «2 h 05 min» from seconds.
    nonisolated static func duration(_ s: Double) -> String {
        let m = Int(s / 60)
        return m >= 60 ? String(format: "%d h %02d min", m / 60, m % 60) : "\(m) min"
    }

    private func slice() {
        busy = true
        percent = 0; stageText = ""
        let job = Engine.SliceJob()
        self.job = job
        // A filament gives the temperatures: without one the engine refuses the G-code (nozzle heater at 0).
        let fallback = Engine.catalog.flatMap { c in model.flatMap { c.filamentsFor(p: $0).first?.fullName } }
        let (p, n, f, chosen) = (printer, nozzle, filament.isEmpty ? fallback : filament, opened)
        let panel = PanelModel.shared.bridge
        Task.detached {
            let dir = FileManager.default.temporaryDirectory
            let model = chosen ?? dir.appendingPathComponent("cube20.stl")
            if chosen == nil { try? Engine.cubeStl().write(to: model, atomically: true, encoding: .utf8) }
            let (config, why) = Engine.profile(printer: p, nozzle: n, filament: f)
            let text: String
            var made: URL?
            if let profile = config {
                // The panel's changes (Print and Material) over the printer's profile.
                let config = profile.deletingPathExtension().appendingPathExtension("changed.json")
                var applied = false
                if let panel, let text = try? String(contentsOf: profile, encoding: .utf8) {
                    applied = (try? panel.over(profileJson: text).write(to: config, atomically: true, encoding: .utf8)) != nil
                }
                let settings = applied ? config : profile
                let out = model.deletingPathExtension().appendingPathExtension("gcode")
                // The engine's library first: progress, cancel and the memory check (rules 12 and 19).
                let outcome = job.run(model: model, settings: (try? String(contentsOf: settings, encoding: .utf8)) ?? "{}", gcode: out) { pc, stage in
                    Task { @MainActor in percent = pc; stageText = stage }
                }
                var summary: String?
                var failure: String?
                switch outcome {
                case .done(let json): summary = json; made = out
                case .cancelled: failure = String(localized: "Cancelled.")
                case .tooLarge(let needed, let available):
                    failure = String(format: String(localized: "This model needs %d MB and the phone has %d MB free: slice it in Tisma Cloud."),
                                     Int(needed / 1_000_000), Int(available / 1_000_000))
                case .failed(let why): failure = why
                case .refused(let why):
                    // The command line, as before, when the library refuses (its exit code says why).
                    print("ENGINE-SLICE library refused: \(why)")
                    let (r, o, sum) = Engine.slice(model: model, config: settings)
                    if r.code == 0 { made = o; summary = sum } else { failure = "exit \(r.code)\n\(r.err)" }
                }
                if let s = summary.flatMap({ EngineBridge.shared.summary(json: $0) }), made != nil {
                    let time = s.printTime.isEmpty ? Self.duration(s.printTimeS) : s.printTime
                    text = "\(time) · \(String(format: "%.2f m", s.filamentMm / 1000)) · \(String(format: "%.1f g", s.filamentG))"
                        + (s.cost >= 0 ? " · " + String(format: "%.2f", s.cost) : "") + " · \(s.layers) layers"
                } else {
                    text = failure ?? "?"
                }
            } else {
                text = "No profile: \(why)"
            }
            await MainActor.run { result = text; gcode = made; busy = false; self.job = nil }
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
        let server = Prefs.server
        let why = await Task.detached { LibraryBridge.shared.refresh(server: server, projectKeyHex: LibraryView.projectKey) }.value
        problem = why
        items = LibraryBridge.shared.search(query: query)
    }
}
