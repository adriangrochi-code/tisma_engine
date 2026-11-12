import Security
import SwiftUI
import TismaShared

/// «Impresora» (printer_connections.md): one printer on the network, through the shared Kotlin clients. The key or
/// password lives in the Keychain, never in plain text (mobile_app.md, rule 14 and MA8, on the iPhone).
struct PrinterView: View {
    @AppStorage("printer.kind") private var kind = "moonraker"
    @AppStorage("printer.address") private var address = ""
    @AppStorage("printer.user") private var user = ""
    @State private var secret = Keychain.read("printer.secret") ?? ""
    @State private var line: PrinterLine?
    @State private var problem: String?
    @State private var tested: String?

    var body: some View {
        NavigationStack {
            Form {
                Section("Connection") {
                    Picker("Kind", selection: $kind) {
                        ForEach(PrinterBridge.shared.kinds, id: \.self) { Text(Self.title($0)).tag($0) }
                    }
                    TextField("Address", text: $address)
                        .textInputAutocapitalization(.never).autocorrectionDisabled().keyboardType(.URL)
                    if kind == "prusalink" { TextField("User", text: $user).textInputAutocapitalization(.never) }
                    SecureField(kind == "bambu" ? "Access code" : kind == "octoprint" || kind == "moonraker" ? "API key" : "Password", text: $secret)
                        .onChange(of: secret) { _, v in Keychain.write("printer.secret", v) }
                    Button("Test") { run { tested = try PrinterBridge.shared.test(kind: $0.0, address: $0.1, user: $0.2, secret: $0.3) } }
                        .disabled(address.isEmpty)
                    if let tested { Text(tested).foregroundStyle(.secondary) }
                }
                if let line {
                    Section("State") {
                        LabeledContent("State", value: line.state)
                        if !line.file.isEmpty { LabeledContent("File", value: line.file) }
                        if line.percent >= 0 { ProgressView(value: line.percent, total: 100) }
                        if !line.nozzle.isEmpty { LabeledContent("Nozzle", value: line.nozzle) }
                        if !line.bed.isEmpty { LabeledContent("Bed", value: line.bed) }
                        HStack {
                            Button("Pause") { run { try PrinterBridge.shared.pause(kind: $0.0, address: $0.1, user: $0.2, secret: $0.3) } }
                            Button("Resume") { run { try PrinterBridge.shared.resume(kind: $0.0, address: $0.1, user: $0.2, secret: $0.3) } }
                            Button("Stop", role: .destructive) { run { try PrinterBridge.shared.cancel(kind: $0.0, address: $0.1, user: $0.2, secret: $0.3) } }
                        }.buttonStyle(.bordered)
                    }
                }
                if let problem { Section { Text(problem).foregroundStyle(.red) } }
            }
            .navigationTitle("Printer")
            // Asked every 5 s only while the screen is shown (as Android's WhileVisible).
            .task(id: address + kind) { await poll() }
        }
    }

    private func poll() async {
        while !Task.isCancelled && !address.isEmpty {
            let a = (kind, address, user, secret)
            let result: Result<PrinterLine, Error> = await Task.detached {
                Result { try PrinterBridge.shared.status(kind: a.0, address: a.1, user: a.2, secret: a.3) }
            }.value
            switch result {
            case .success(let l): line = l; problem = nil
            case .failure(let e): problem = e.localizedDescription
            }
            try? await Task.sleep(for: .seconds(5))
        }
    }

    /// Runs a printer call off the main thread and shows its error, if any.
    private func run(_ call: @escaping ((String, String, String, String)) throws -> Void) {
        let a = (kind, address, user, secret)
        Task.detached {
            do { try call(a); await MainActor.run { problem = nil } }
            catch { let m = error.localizedDescription; await MainActor.run { problem = m } }
        }
    }

    static func title(_ kind: String) -> String {
        switch kind {
        case "moonraker": return "Klipper (Moonraker)"
        case "octoprint": return "OctoPrint"
        case "prusalink": return "PrusaLink"
        case "duet": return "Duet"
        case "bambu": return "Bambu Lab (LAN)"
        default: return kind
        }
    }
}

/// The app's secrets in the iPhone's Keychain (this device only, after the first unlock).
enum Keychain {
    static func write(_ key: String, _ value: String) {
        let base: [String: Any] = [kSecClass as String: kSecClassGenericPassword, kSecAttrAccount as String: key]
        SecItemDelete(base as CFDictionary)
        guard !value.isEmpty else { return }
        var add = base
        add[kSecValueData as String] = Data(value.utf8)
        add[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        SecItemAdd(add as CFDictionary, nil)
    }

    static func read(_ key: String) -> String? {
        let q: [String: Any] = [kSecClass as String: kSecClassGenericPassword, kSecAttrAccount as String: key,
                                kSecReturnData as String: true, kSecMatchLimit as String: kSecMatchLimitOne]
        var out: AnyObject?
        guard SecItemCopyMatching(q as CFDictionary, &out) == errSecSuccess, let d = out as? Data else { return nil }
        return String(data: d, encoding: .utf8)
    }
}
