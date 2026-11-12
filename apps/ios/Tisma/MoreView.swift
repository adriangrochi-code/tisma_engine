import SwiftUI
import TismaShared

/// «Más»: the Tisma Cloud account (cloud.md, rules 1 to 5), through the shared Kotlin client.
struct MoreView: View {
    @State private var server = CloudBridge.shared.defaultServer
    @State private var email = ""
    @State private var password = ""
    @State private var line: String?
    @State private var problem: String?
    @State private var busy = false
    @State private var serviceNotice: String?

    var body: some View {
        NavigationStack {
            Form {
                if let serviceNotice {
                    Section { Label(serviceNotice, systemImage: "exclamationmark.triangle").foregroundStyle(.orange) }
                }
                Section("Tisma Cloud") {
                    TextField("Server", text: $server)
                        .textInputAutocapitalization(.never).autocorrectionDisabled().keyboardType(.URL)
                    TextField("Email", text: $email)
                        .textInputAutocapitalization(.never).autocorrectionDisabled().keyboardType(.emailAddress)
                    SecureField("Password", text: $password)
                    Button("Sign in") { signIn() }.disabled(busy || email.isEmpty || password.isEmpty)
                    if let line { Text(line) }
                    if let problem { Text(problem).foregroundStyle(.red) }
                }
            }
            .navigationTitle("More")
            .task(id: server) { await checkService() }
        }
    }

    /// cloud.md 4a: asked when the cloud panel opens; only a notice, never a block.
    private func checkService() async {
        let s = server
        let spanish = Locale.current.language.languageCode?.identifier == "es"
        let notice = await Task.detached { CloudBridge.shared.serviceNotice(server: s, spanish: spanish) }.value
        serviceNotice = notice
    }

    private func signIn() {
        busy = true; problem = nil
        let (s, e, p) = (server, email, password)
        Task.detached {
            do {
                let token = try CloudBridge.shared.login(server: s, email: e, password: p, device: "iPhone")
                let text = try CloudBridge.shared.accountLine(server: s, token: token)
                await MainActor.run { line = text; busy = false }
            } catch {
                let message = error.localizedDescription
                await MainActor.run { problem = message; busy = false }
            }
        }
    }
}
