import SwiftUI
import TismaShared
import UIKit

/// «Informar un problema» (cloud.md, rule 11; mobile_app.md, rule 9; parity P6): the kind, a title and what happened,
/// sent to Tisma Cloud as `app` `ios`. The version and the system go only when the person accepts it.
struct ReportView: View {
    @State private var kind = "bug"
    @State private var title = ""
    @State private var description = ""
    @State private var steps = ""
    @State private var expected = ""
    @State private var actual = ""
    @State private var withSystem = true
    @State private var busy = false
    @State private var answer: String?
    @State private var problem: String?

    var body: some View {
        Form {
            Picker("Kind", selection: $kind) {
                Text("Something does not work").tag("bug")
                Text("The app closed").tag("crash")
                Text("A suggestion").tag("suggestion")
            }
            Section {
                TextField("Title", text: $title).accessibilityIdentifier("report_title")
                TextField("What happened", text: $description, axis: .vertical)
                if kind != "suggestion" {
                    TextField("Steps to make it happen", text: $steps, axis: .vertical)
                    TextField("What you expected", text: $expected, axis: .vertical)
                    TextField("What happened instead", text: $actual, axis: .vertical)
                }
            }
            Toggle("Send the app's version and the system", isOn: $withSystem)
            Section {
                Button(busy ? String(localized: "Sending…") : String(localized: "Send")) { send() }
                    .disabled(busy || title.trimmingCharacters(in: .whitespaces).count < 3)
                    .accessibilityIdentifier("report_send")
                if let answer { Text(answer) }
                if let problem { Text(problem).foregroundStyle(.red) }
            }
        }
        .navigationTitle("Report a problem")
    }

    private func send() {
        busy = true; answer = nil; problem = nil
        let version = withSystem ? (Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "") : ""
        let os = withSystem ? "iOS \(UIDevice.current.systemVersion) \(UIDevice.current.model)" : ""
        let token = Keychain.read("cloud.token") ?? ""
        let (k, t, d, s, e, a) = (kind, title, description, steps, expected, actual)
        Task.detached {
            do {
                let similar = try CloudBridge.shared.report(server: CloudBridge.shared.defaultServer, token: token, kind: k, title: t,
                                                            description: d, steps: s, expected: e, actual: a, appVersion: version, os: os)
                let text = similar > 0 ? String(format: String(localized: "Sent. %d people reported something alike."), Int(similar))
                    : String(localized: "Sent. Thank you.")
                await MainActor.run { answer = text; busy = false }
            } catch {
                let m = error.localizedDescription
                await MainActor.run { problem = m; busy = false }
            }
        }
    }
}
