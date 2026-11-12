import SwiftUI

/// Tisma for iPhone: the same sections as the Android app (mobile_app.md, rule 5). Slicing waits for the engine as a
/// library (apps/ios/PLAN.md, step 4); until then the plate says so.
@main
struct TismaApp: App {
    var body: some Scene {
        WindowGroup { RootView() }
    }
}

struct RootView: View {
    var body: some View {
        TabView {
            PlateView().tabItem { Label("Plate", systemImage: "square.grid.3x3") }
            LibraryView().tabItem { Label("Library", systemImage: "books.vertical") }
            PrinterView().tabItem { Label("Printer", systemImage: "printer") }
            MoreView().tabItem { Label("More", systemImage: "ellipsis.circle") }
        }
    }
}

struct PlateView: View {
    var body: some View {
        ContentUnavailableView("Slicing on the iPhone is coming",
                               systemImage: "cube",
                               description: Text("The engine is not in this build yet. Library, printers and the cloud work."))
    }
}

struct LibraryView: View {
    @State private var query = ""
    var body: some View {
        NavigationStack {
            List {}
                .overlay { ContentUnavailableView("No models yet", systemImage: "books.vertical") }
                .searchable(text: $query)
                .navigationTitle("Library")
        }
    }
}

struct PrinterView: View {
    var body: some View {
        NavigationStack {
            ContentUnavailableView("No printers", systemImage: "printer",
                                   description: Text("Klipper, OctoPrint, PrusaLink and Duet come first."))
                .navigationTitle("Printer")
        }
    }
}
