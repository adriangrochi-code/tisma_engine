import SwiftUI
import TismaShared

/// The app's preferences (mobile_app.md, rule 9; parity P7), kept in the app's defaults.
enum Prefs {
    /// The Tisma Cloud server: the person's, or Tisma's own when empty.
    static var server: String {
        let s = UserDefaults.standard.string(forKey: "cloud.server")?.trimmingCharacters(in: .whitespaces) ?? ""
        return s.isEmpty ? CloudBridge.shared.defaultServer : s
    }
}

/// «Preferencias»: the server (another Tisma Cloud, such as a company's own), applied at the next start.
struct PrefsView: View {
    @AppStorage("cloud.server") private var server = ""

    var body: some View {
        Form {
            Section {
                TextField(CloudBridge.shared.defaultServer, text: $server)
                    .textInputAutocapitalization(.never).autocorrectionDisabled().keyboardType(.URL)
                    .accessibilityIdentifier("pref_server")
            } header: {
                Text("Tisma Cloud server")
            } footer: {
                Text("Empty: Tisma's own. The account uses the new server from the next start.")
            }
        }
        .navigationTitle("Preferences")
    }
}

/// «Créditos y fuentes» (credits.md, rule 4; parity P7): the functions based on a source, and the libraries the app
/// carries with their notices, from the engine's credits and the iPhone's own list (the same parser as Android).
struct CreditsView: View {
    private static let credits: Credits? = {
        func text(_ name: String) -> String? {
            Bundle.main.url(forResource: name, withExtension: "json").flatMap { try? String(contentsOf: $0, encoding: .utf8) }
        }
        guard let engine = text("credits"), let ios = text("credits_ios") else { return nil }
        let spanish = Locale.current.language.languageCode?.identifier == "es"
        return try? Credits.companion.parse(engineJson: engine, androidJson: ios, spanish: spanish)
    }()

    var body: some View {
        List {
            if let c = Self.credits {
                Section("Based on") {
                    ForEach(c.features, id: \.name) { f in
                        VStack(alignment: .leading) {
                            Text(f.name)
                            Text("\(f.source) · \(f.author) · \(f.license)").font(.caption).foregroundStyle(.secondary)
                        }
                    }
                }
                Section("Libraries") {
                    ForEach(c.libraries, id: \.name) { l in
                        NavigationLink {
                            NoticeView(library: l)
                        } label: {
                            VStack(alignment: .leading) {
                                Text(l.name)
                                Text("\(l.version) · \(l.license)").font(.caption).foregroundStyle(.secondary)
                            }
                        }
                    }
                }
            } else {
                Text("The credits are missing from this build.").foregroundStyle(.red)
            }
        }
        .navigationTitle("Credits and sources")
        .accessibilityIdentifier("credits")
    }
}

private struct NoticeView: View {
    let library: Credits.Library

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text(library.url).font(.footnote).textSelection(.enabled)
                ForEach(library.notices, id: \.self) { n in
                    Text(Self.notice(n)).font(.caption.monospaced()).textSelection(.enabled)
                }
            }
            .padding()
        }
        .navigationTitle(library.name)
    }

    static func notice(_ name: String) -> String {
        let url = Bundle.main.resourceURL?.appendingPathComponent("licenses").appendingPathComponent(name)
        return url.flatMap { try? String(contentsOf: $0, encoding: .utf8) } ?? name
    }
}
