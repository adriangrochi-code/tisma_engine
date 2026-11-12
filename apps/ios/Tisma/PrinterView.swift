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
    @ObservedObject private var watcher = PrintWatcher.shared
    @State private var problem: String?
    private var line: PrinterLine? { watcher.line }
    @State private var tested: String?
    @StateObject private var discovery = PrinterDiscovery()
    @State private var showCamera = false
    @State private var shot: UIImage?
    @State private var cameraProblem: String?

    var body: some View {
        NavigationStack {
            Form {
                // printer_connections.md, «Búsqueda en la red» (parity P17): a tap fills in the address.
                if !discovery.found.isEmpty {
                    Section("On the network") {
                        ForEach(discovery.found, id: \.url) { f in
                            Button {
                                kind = f.kind; address = f.url
                            } label: {
                                VStack(alignment: .leading) {
                                    Text(f.name).foregroundStyle(.primary)
                                    Text("\(Self.title(f.kind)) · \(f.url)\(f.model.isEmpty ? "" : " · " + f.model)").font(.caption).foregroundStyle(.secondary)
                                }
                            }
                        }
                    }
                }
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
                // mobile_app.md, rule 14 (parity P18): the printer's camera, read while it is shown.
                if !address.isEmpty {
                    Section("Camera") {
                        Toggle("Show the camera", isOn: $showCamera).accessibilityIdentifier("printer_camera")
                        if showCamera {
                            if let shot { Image(uiImage: shot).resizable().scaledToFit() }
                            else if let cameraProblem { Text(cameraProblem).foregroundStyle(.secondary) }
                            else { ProgressView() }
                        }
                    }
                    .task(id: showCamera) { await followCamera() }
                }
                if let problem = problem ?? watcher.problem { Section { Text(problem).foregroundStyle(.red) } }
            }
            .navigationTitle("Printer")
            .onAppear { discovery.start(saved: address) }
            .onDisappear { discovery.stop() }
            // A changed printer is read at once; the watcher keeps reading it while the app is open.
            .task(id: address + kind) { await watcher.readOnce() }
        }
    }

    /// A new image every 2 s (or as a Bambu Lab sends them) while the camera is shown.
    private func followCamera() async {
        defer { PrinterBridge.shared.stopCamera(); shot = nil }
        guard showCamera else { return }
        let a = (kind, address, user, secret)
        while !Task.isCancelled && showCamera {
            let r: Result<CameraShot?, Error> = await Task.detached {
                Result { try PrinterBridge.shared.cameraShot(kind: a.0, address: a.1, user: a.2, secret: a.3) }
            }.value
            switch r {
            case .success(let s?): shot = Self.image(s); cameraProblem = nil
            case .success(nil): cameraProblem = String(localized: "This printer has no camera."); return
            case .failure(let e): cameraProblem = e.localizedDescription
            }
            try? await Task.sleep(for: .seconds(2))
        }
    }

    /// The JPEG turned and flipped as the printer's settings say.
    static func image(_ s: CameraShot) -> UIImage? {
        guard let img = UIImage(data: s.jpeg as Data), let cg = img.cgImage else { return nil }
        // A vertical flip is a horizontal one turned half a turn; both flips are half a turn.
        var turn = Int(s.rotation)
        if s.flipVertical { turn += 180 }
        let mirrored = s.flipHorizontal != s.flipVertical
        let base: UIImage.Orientation
        switch (turn % 360, mirrored) {
        case (90, false): base = .right
        case (180, false): base = .down
        case (270, false): base = .left
        case (0, true): base = .upMirrored
        case (90, true): base = .rightMirrored
        case (180, true): base = .downMirrored
        case (270, true): base = .leftMirrored
        default: base = .up
        }
        return UIImage(cgImage: cg, scale: 1, orientation: base)
    }

    /// Runs a printer call off the main thread and shows its error, if any.
    private func run(_ call: @escaping ((String, String, String, String)) throws -> Void) {
        let a = (kind, address, user, secret)
        Task.detached {
            do { try call(a); await MainActor.run { problem = nil } }
            catch { let m = error.localizedDescription; await MainActor.run { problem = m } }
        }
    }

    static func title(_ kind: String) -> String { PrinterKinds.title(kind) }
}

/// The printer kinds by the names the person reads.
enum PrinterKinds {
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
