import SwiftUI
import TismaShared

/// The first-use assistant (mobile_app.md, rule 11; MA1): the printer by its model (searchable, accents ignored),
/// its nozzle and a filament that fits it, from the engine's catalog. It shows once, on the first start, and can be
/// opened again from More.
struct WizardView: View {
    @Environment(\.dismiss) private var dismiss
    @AppStorage("wizard.done") private var done = false
    @AppStorage("plate.printer") private var printer = ""
    @AppStorage("plate.nozzle") private var nozzle = 0.4
    @AppStorage("plate.filament") private var filament = ""
    @State private var query = ""
    @State private var step = 0

    private var catalog: Catalog? { Engine.catalog }
    private var model: Catalog.PrinterModel? { catalog?.printer(name: printer) }

    var body: some View {
        NavigationStack {
            Group {
                switch step {
                case 0: printerStep
                case 1: nozzleStep
                default: filamentStep
                }
            }
            .navigationTitle("Welcome to Tisma")
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Later") { finish() } }
            }
        }
        .interactiveDismissDisabled()
    }

    private var printerStep: some View {
        List((catalog?.search(query: query) ?? []), id: \.name) { p in
            Button {
                printer = p.name
                nozzle = p.nozzles.first?.doubleValue ?? 0.4
                step = p.nozzles.count > 1 ? 1 : 2
            } label: {
                VStack(alignment: .leading) {
                    Text(p.model).foregroundStyle(.primary)
                    Text(p.vendor).font(.caption).foregroundStyle(.secondary)
                }
            }
            .accessibilityIdentifier("wizard_printer")
        }
        .searchable(text: $query, prompt: "Your printer's model")
        .safeAreaInset(edge: .top) { Text("Which printer do you have?").font(.headline).padding(.top) }
    }

    private var nozzleStep: some View {
        List(model?.nozzles.map { $0.doubleValue } ?? [], id: \.self) { n in
            Button(String(format: "%.2f mm", n)) { nozzle = n; step = 2 }
        }
        .safeAreaInset(edge: .top) { Text("Which nozzle?").font(.headline).padding(.top) }
    }

    private var filamentStep: some View {
        List(catalog?.filamentsFor(p: model) ?? [], id: \.fullName) { f in
            Button {
                filament = f.fullName
                finish()
            } label: {
                VStack(alignment: .leading) {
                    Text(f.name).foregroundStyle(.primary)
                    Text("\(f.vendor) · \(f.material)").font(.caption).foregroundStyle(.secondary)
                }
            }
            .accessibilityIdentifier("wizard_filament")
        }
        .safeAreaInset(edge: .top) { Text("Which filament?").font(.headline).padding(.top) }
    }

    private func finish() {
        done = true
        dismiss()
    }
}
