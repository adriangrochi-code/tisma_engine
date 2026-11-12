import SwiftUI
import TismaShared

/// The panel's state for the whole app: the layout in the phone's language and the person's changes, kept between
/// runs (mobile_app.md, rule 7; settings_panel.md).
@MainActor
final class PanelModel: ObservableObject {
    static let shared = PanelModel()
    let bridge: PanelBridge?
    @Published private(set) var version = 0

    init() {
        let lang = Locale.current.language.languageCode?.identifier == "es" ? "es" : "en"
        let text = Bundle.main.url(forResource: "layout_\(lang)", withExtension: "json").flatMap { try? String(contentsOf: $0, encoding: .utf8) }
        bridge = text.map { PanelBridge(layoutJson: $0, changesJson: UserDefaults.standard.string(forKey: "panel.changes") ?? "{}") }
    }

    func changed() {
        if let b = bridge { UserDefaults.standard.set(b.changesJson(), forKey: "panel.changes") }
        version += 1
    }
}

/// «Impresión» or «Material»: the panel's sections at the chosen level, one card per section.
struct SettingsView: View {
    let tab: String
    let title: LocalizedStringKey
    @ObservedObject private var panel = PanelModel.shared
    @AppStorage("panel.level") private var level = 0

    var body: some View {
        NavigationStack {
            List {
                Picker("Level", selection: $level) {
                    Text("Basic").tag(0); Text("Normal").tag(1); Text("Advanced").tag(2)
                }
                .pickerStyle(.segmented)
                if let b = panel.bridge {
                    let _ = panel.version
                    ForEach(b.sections(tab: tab, level: Int32(level)), id: \.id) { s in
                        Section(s.title) { ForEach(s.rows, id: \.key) { RowView(row: $0) } }
                    }
                } else {
                    Text("The settings layout is missing from this build.").foregroundStyle(.red)
                }
            }
            .navigationTitle(title)
            .toolbar {
                ToolbarItemGroup(placement: .keyboard) {
                    Spacer()
                    Button("Done") {
                        UIApplication.shared.sendAction(#selector(UIResponder.resignFirstResponder), to: nil, from: nil, for: nil)
                    }
                }
            }
        }
    }
}

private struct RowView: View {
    let row: PanelRow
    @ObservedObject private var panel = PanelModel.shared
    @State private var text = ""
    @State private var bad = false
    @FocusState private var editing: Bool

    var body: some View {
        switch row.kind {
        case "toggle":
            Toggle(row.label, isOn: Binding(get: { panel.bridge?.isOn(key: row.key) ?? false },
                                            set: { panel.bridge?.setToggle(key: row.key, on: $0); panel.changed() }))
        case "choice":
            Picker(row.label, selection: Binding(get: { row.value }, set: { v in _ = panel.bridge?.set(key: row.key, text: v); panel.changed() })) {
                ForEach(row.options, id: \.self) { Text($0).tag($0) }
            }
        case "readOnly":
            LabeledContent(row.label, value: row.value)
        default:
            HStack {
                Text(row.label)
                Spacer()
                TextField(row.value, text: $text)
                    .multilineTextAlignment(.trailing)
                    // mobile_app.md, rule 17: numbers with the phone's number pad (a percent may be typed too).
                    .keyboardType(row.kind == "text" ? .default : row.kind == "number" && !signed ? .decimalPad : .numbersAndPunctuation)
                    .foregroundStyle(bad ? .red : .primary)
                    .focused($editing)
                    .onSubmit { commit() }
                    // The number pad has no return key: the value is kept when the field is left.
                    .onChange(of: editing) { _, now in if !now && text != row.value { commit() } }
                    .frame(maxWidth: 140)
                if !row.unit.isEmpty { Text(row.unit).foregroundStyle(.secondary) }
            }
            .onAppear { text = row.value }
        }
    }

    /// A value that may go below zero (an offset, or one that already is) keeps the keyboard with the minus sign.
    private var signed: Bool { row.value.hasPrefix("-") || row.key.contains("offset") }

    private func commit() {
        bad = !(panel.bridge?.set(key: row.key, text: text) ?? false)
        if !bad { panel.changed() }
    }
}
