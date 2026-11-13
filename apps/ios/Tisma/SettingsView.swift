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
        bridge?.setCustomLine(line: UserDefaults.standard.string(forKey: "panel.custom") ?? "")
    }

    func changed() {
        if let b = bridge {
            UserDefaults.standard.set(b.changesJson(), forKey: "panel.changes")
            UserDefaults.standard.set(b.customLine, forKey: "panel.custom")
        }
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
                    Text("Basic").tag(0); Text("Normal").tag(1); Text("Advanced").tag(2); Text("Custom").tag(3)
                }
                .pickerStyle(.segmented)
                if let b = panel.bridge {
                    let _ = panel.version
                    // settings_panel.md, rule 7: the Custom view shows the rows the person chose.
                    if level == 3 {
                        NavigationLink("Choose settings…") { CustomPicker(tab: tab) }.accessibilityIdentifier("panel_choose")
                    }
                    let sections = b.sections(tab: tab, level: Int32(level))
                    if sections.isEmpty && level == 3 {
                        Text("Choose the settings to show with «Choose settings…».").foregroundStyle(.secondary)
                    }
                    ForEach(sections, id: \.id) { s in
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
            // Large text (Dynamic Type): the label goes above the field instead of being cut.
            ViewThatFits(in: .horizontal) {
                HStack { Text(row.label); Spacer(); field }
                VStack(alignment: .leading) { Text(row.label); field }
            }
            .onAppear { text = row.value }
        }
    }

    private var field: some View {
        HStack {
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
    }

    /// A value that may go below zero (an offset, or one that already is) keeps the keyboard with the minus sign.
    private var signed: Bool { row.value.hasPrefix("-") || row.key.contains("offset") }

    private func commit() {
        bad = !(panel.bridge?.set(key: row.key, text: text) ?? false)
        if !bad { panel.changed() }
    }
}

/// Choosing the Custom view's rows: every row of the tab, by section, with a check.
private struct CustomPicker: View {
    let tab: String
    @ObservedObject private var panel = PanelModel.shared

    var body: some View {
        List {
            if let b = panel.bridge {
                let _ = panel.version
                ForEach(b.allSections(tab: tab), id: \.id) { s in
                    Section(s.title) {
                        ForEach(s.rows, id: \.key) { r in
                            Button { b.toggleCustom(key: r.key); panel.changed() } label: {
                                HStack {
                                    Text(r.label).foregroundStyle(.primary)
                                    Spacer()
                                    if b.isCustom(key: r.key) { Image(systemName: "checkmark").foregroundStyle(Color.accentColor) }
                                }
                            }
                            .accessibilityAddTraits(b.isCustom(key: r.key) ? .isSelected : [])
                            .accessibilityIdentifier("pick_" + r.key)
                        }
                    }
                }
            }
        }
        .navigationTitle("Choose settings…")
    }
}
