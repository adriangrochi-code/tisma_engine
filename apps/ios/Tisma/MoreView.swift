import SwiftUI
import TismaShared

/// The session's token in the Keychain (cloud.md, rule 3), for the shared account model.
final class KeychainTokens: NSObject, TokenStore {
    func read() -> String? { Keychain.read("cloud.token") }
    func write(token: String?) { Keychain.write("cloud.token", token ?? "") }
}

/// The shared Kotlin AccountModel as a SwiftUI observable: its state arrives on the main thread.
@MainActor
final class AccountScreenModel: ObservableObject {
    @Published private(set) var state: AccountState?
    let model = CloudBridge.shared.accountModel(tokens: KeychainTokens())
    private var stop: (() -> Void)?

    init() {
        stop = model.watch { [weak self] s in
            Task { @MainActor in self?.state = s }
        }
    }

    deinit { stop?() }
}

/// «Más»: the Tisma Cloud account (cloud.md, rules 1 to 5a, 4a), through the shared account model.
struct MoreView: View {
    @StateObject private var account = AccountScreenModel()
    @State private var email = ""
    @State private var password = ""

    private var spanish: Bool { Locale.current.language.languageCode?.identifier == "es" }

    var body: some View {
        NavigationStack {
            Form {
                if let st = account.state?.service, st.worthNotice {
                    Section {
                        Label(st.label(spanish: spanish) + (st.message(spanish: spanish).isEmpty ? "" : ": " + st.message(spanish: spanish)),
                              systemImage: "exclamationmark.triangle").foregroundStyle(.orange)
                    }
                }
                Section("Tisma Cloud") {
                    if account.state?.signedIn == true {
                        if let a = account.state?.account {
                            LabeledContent("Email", value: a.email)
                            LabeledContent("Plan", value: a.plan)
                            LabeledContent("Hours left", value: a.unlimited ? "∞" : String(format: "%.1f", a.hoursLeft))
                        }
                        Button("Sign out", role: .destructive) { account.model.signOut() }
                    } else {
                        TextField("Email", text: $email)
                            .textInputAutocapitalization(.never).autocorrectionDisabled().keyboardType(.emailAddress)
                        SecureField("Password", text: $password)
                        Button("Sign in") { account.model.signIn(email: email, password: password); password = "" }
                            .disabled(account.state?.busy == true || email.isEmpty || password.isEmpty)
                    }
                    if let p = account.state?.problem { Text(p.detail).foregroundStyle(.red) }
                }
                if let plans = account.state?.plans, !plans.isEmpty {
                    Section("Plans") {
                        ForEach(plans, id: \.name) { p in Text(p.name) }
                    }
                }
            }
            .navigationTitle("More")
            .task { account.model.refresh() }
        }
    }
}
