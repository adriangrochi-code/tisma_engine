import SwiftUI
import TismaShared

/// The free profile pack on the iPhone (profile_pack.md; mobile_app.md, rule 11; parity P10): the engine plans and
/// installs it into the app's data folder, the app downloads what the plan names (the same steps as Android).
enum Pack {
    /// `<Application Support>/tisma`; the pack lives in its `pack` folder.
    static var dataDir: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("tisma")
        try? FileManager.default.createDirectory(at: base, withIntermediateDirectories: true)
        return base
    }

    static var packDir: URL { dataDir.appendingPathComponent("pack") }

    static var installed: Bool { FileManager.default.fileExists(atPath: packDir.appendingPathComponent("index.json").path) }

    struct Failure: Error, LocalizedError {
        let errorDescription: String?
    }

    /// The plan for the data folder (with the downloaded index, the second plan: the rest).
    static func plan(index: URL?) throws -> [PackFile] {
        let r = Engine.run(PackBridge.shared.planArgs(dataDir: dataDir.path, index: index?.path ?? "", vendors: "", url: ""))
        print("ENGINE-PACK plan exit=\(r.code) err=\(r.err.suffix(200))")
        guard r.code == 0, let files = PackBridge.shared.plan(json: r.out) else {
            throw Failure(errorDescription: r.err.split(separator: "\n").last.map(String.init) ?? "exit \(r.code)")
        }
        return files
    }

    /// Downloads and installs the whole pack (every vendor on the first time, the installed ones after); its printers.
    static func update(progress: @escaping (Int) -> Void) throws -> Int {
        let staging = dataDir.appendingPathComponent("pack-staging")
        try? FileManager.default.removeItem(at: staging)
        try FileManager.default.createDirectory(at: staging, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: staging) }
        for f in try plan(index: nil) { try PackBridge.shared.download(file: f, dir: staging.path) }
        progress(10)
        let index = staging.appendingPathComponent("index.json")
        guard FileManager.default.fileExists(atPath: index.path) else { throw Failure(errorDescription: "The index did not arrive") }
        let rest = try plan(index: index).filter { $0.name != "index.json" }
        for (i, f) in rest.enumerated() {
            try PackBridge.shared.download(file: f, dir: staging.path)
            progress(10 + 80 * (i + 1) / max(rest.count, 1))
        }
        let r = Engine.run(PackBridge.shared.installArgs(dataDir: dataDir.path, staging: staging.path))
        print("ENGINE-PACK install exit=\(r.code) err=\(r.err.suffix(200))")
        guard r.code == 0 else { throw Failure(errorDescription: r.err.split(separator: "\n").last.map(String.init) ?? "exit \(r.code)") }
        progress(100)
        return printers().count
    }

    /// The installed pack's printers (`pack list`), or none.
    static func printers() -> [PackPrinter] {
        guard installed else { return [] }
        let r = Engine.run(PackBridge.shared.listArgs(dataDir: dataDir.path))
        return r.code == 0 ? PackBridge.shared.printers(json: r.out) : []
    }
}

/// «Paquete de perfiles» in More: what is installed, and a way to get it or bring it up to date.
struct PackView: View {
    @State private var count = Pack.printers().count
    @State private var busy = false
    @State private var percent = 0
    @State private var message: String?
    @State private var problem: String?

    var body: some View {
        Form {
            Section {
                LabeledContent("Installed", value: Pack.installed ? String(format: String(localized: "%d printers"), count) : String(localized: "No"))
                Button(Pack.installed ? String(localized: "Update the pack") : String(localized: "Download the pack")) { update() }
                    .disabled(busy)
                    .accessibilityIdentifier("pack_update")
                if busy { ProgressView(value: Double(percent), total: 100) }
                if let message { Text(message) }
                if let problem { Text(problem).foregroundStyle(.red) }
            } footer: {
                Text("The free profiles of many printers and filaments, and the offline help. Downloaded over the internet once, then used without it.")
            }
        }
        .navigationTitle("Profile pack")
    }

    private func update() {
        busy = true; percent = 0; problem = nil; message = nil
        Task.detached {
            do {
                let n = try Pack.update { p in Task { @MainActor in percent = p } }
                await MainActor.run { count = n; message = String(localized: "The pack is up to date."); busy = false }
            } catch {
                let m = error.localizedDescription
                await MainActor.run { problem = m; busy = false }
            }
        }
    }
}

/// «Ayuda» (assistant.md; mobile_app.md, rule 16; parity P21): problems, procedures, glossary, materials and
/// calibration guides from the installed pack, without a connection, each part with its sources.
struct HelpView: View {
    private let sections = PackBridge.shared.help(dir: Pack.packDir.appendingPathComponent("knowledge").path)
    @State private var query = ""

    var body: some View {
        List {
            if sections.isEmpty {
                Text("The help comes with the profile pack: download it in More › Profile pack.").foregroundStyle(.secondary)
            }
            ForEach(sections, id: \.id) { s in
                let entries = s.entries.filter { query.isEmpty || $0.title.localizedCaseInsensitiveContains(query) || $0.text.localizedCaseInsensitiveContains(query) }
                if !entries.isEmpty {
                    Section(Self.title(s.id)) {
                        ForEach(Array(entries.enumerated()), id: \.offset) { _, e in
                            NavigationLink(e.title) { HelpEntryView(entry: e, sources: s.sources) }
                        }
                    }
                }
            }
        }
        .searchable(text: $query)
        .navigationTitle("Help")
    }

    static func title(_ id: String) -> String {
        switch id {
        case "problems": return String(localized: "Problems")
        case "procedures": return String(localized: "Procedures")
        case "glossary": return String(localized: "Glossary")
        case "materials": return String(localized: "Materials")
        default: return String(localized: "Calibration guides")
        }
    }
}

private struct HelpEntryView: View {
    let entry: HelpEntry
    let sources: [String]

    var body: some View {
        List {
            if !entry.text.isEmpty { Text(entry.text) }
            ForEach(Array(entry.lines.enumerated()), id: \.offset) { _, l in Text(l) }
            if !sources.isEmpty {
                Section("Sources") { ForEach(sources, id: \.self) { Text($0).font(.footnote) } }
            }
        }
        .navigationTitle(entry.title)
    }
}
