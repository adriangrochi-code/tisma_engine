import XCTest

/// The release gate's G16 for the iPhone (docs/spec/RELEASE_GATE.md, owner's decision 16-11-2026), on the simulator,
/// through the app's screens: the assistant, opening an STL, an OBJ, a 3MF of its own and one from another program
/// as another app hands them over (G1, the size the plate shows), slicing the 20 mm cube and the 3DBenchy (G2: layers,
/// filament and time > 0) and sending to each simulated printer (G10: it arrives and starts). The CI then checks the
/// G-code's dimensions and that each printer got it whole (tools/gate_ios_check.py), from the files the app left.
///
/// The CI passes the folder with the models as GATE_DIR (TEST_RUNNER_GATE_DIR) and runs the printer simulators on
/// this machine (127.0.0.1:7125–7128). Nothing here is skipped or relaxed (RELEASE_GATE.md, rule 3).
final class ReleaseGateTests: XCTestCase {
    private let app = XCUIApplication()
    private var dir: String { ProcessInfo.processInfo.environment["GATE_DIR"] ?? "" }

    override func setUp() { continueAfterFailure = false }

    func testG16AssistantOpenSliceSend() throws {
        // Its own CI job (release-gate.yml) runs it with GATE_DIR; the app's other test runs leave it out.
        XCTAssertFalse(dir.isEmpty, "GATE_DIR: the folder with the gate's models")
        app.launchArguments += ["--first-start"]
        app.launch()
        assistant()
        // «Open with» starts the app again with its launch arguments: without --first-start from here on, as a phone
        // that went through the assistant once (and it must not come back).
        app.terminate()
        app.launchArguments = ["--ui-test-open"]
        app.launch()
        XCTAssertTrue(app.tabBars.buttons["Plate"].waitForExistence(timeout: 20), "the app starts on its tabs")
        XCTAssertFalse(app.navigationBars["Welcome to Tisma"].exists, "the assistant shows only once")
        open("cube20.stl", size: "20.0 × 20.0 × 20.0 mm")
        open("box.obj", size: "20.0 × 20.0 × 10.0 mm")
        open("box.3mf", size: "20.0 × 20.0 × 10.0 mm")
        open("pi2_prusaslicer.3mf", size: "30.0 × 10.0 × 15.0 mm")
        open("3DBenchy.stl", size: nil)
        slice("3DBenchy")
        open("cube20.stl", size: "20.0 × 20.0 × 20.0 mm")
        slice("cube20")
        for (kind, port, user, secret) in [("moonraker", 7125, "", ""), ("octoprint", 7126, "", "test-key"),
                                           ("prusalink", 7127, "maker", "test-pass"), ("duet", 7128, "", "reprap")] {
            send(kind: kind, port: port, user: user, secret: secret)
        }
    }

    /// MA1: the first start's assistant, a printer and its filament.
    private func assistant() {
        XCTAssertTrue(app.navigationBars["Welcome to Tisma"].waitForExistence(timeout: 20), "the assistant opens")
        app.buttons.matching(identifier: "wizard_printer").firstMatch.tap()
        let nozzle = app.buttons["0.40 mm"]
        if nozzle.waitForExistence(timeout: 2) { nozzle.tap() }
        let filament = app.buttons.matching(identifier: "wizard_filament").firstMatch
        XCTAssertTrue(filament.waitForExistence(timeout: 10), "the assistant offers filaments")
        filament.tap()
        XCTAssertTrue(app.tabBars.buttons["Plate"].waitForExistence(timeout: 10), "the assistant closes")
    }

    /// «Open with» Tisma: the file handed to the app by its address; the plate names it and shows its size.
    private func open(_ name: String, size: String?) {
        // The simulator hands no URL from a UI test to the app (neither a file URL nor its own scheme reached it): the
        // app starts with the file in TISMA_UI_TEST_OPEN and takes it as «Open with» does.
        app.terminate()
        app.launchEnvironment["TISMA_UI_TEST_OPEN"] = URL(fileURLWithPath: dir).appendingPathComponent(name).path
        app.launch()
        let plate = app.tabBars.buttons["Plate"]
        XCTAssertTrue(plate.waitForExistence(timeout: 20), "the app opens \(name)")
        if !plate.isSelected { plate.tap() }
        let model = app.staticTexts["plate_model"]
        XCTAssertTrue(model.waitForExistence(timeout: 30), "the plate names \(name) (on screen: \(screen()))")
        XCTAssertTrue(wait(60) { model.label == name }, "\(name) is on the plate (it shows «\(model.label)»)")
        let info = app.staticTexts["plate_info"]
        XCTAssertTrue(wait(60) { info.exists && (size == nil || info.label.hasPrefix(size!)) },
                      "the size of \(name) on the plate (it shows «\(info.exists ? info.label : "nothing")»)")
    }

    /// Slices from the plate's button; the result line has time, filament and layers above zero.
    private func slice(_ name: String) {
        let button = app.buttons["plate_slice"]
        scrollTo(button)
        button.tap()
        let result = app.staticTexts["plate_result"]
        XCTAssertTrue(wait(900) { result.exists }, "\(name) sliced (on screen: \(screen()))")
        // «17 min · 1.46 m · 4.4 g · 100 layers»
        let numbers = result.label.components(separatedBy: CharacterSet(charactersIn: "0123456789.").inverted)
            .compactMap(Double.init)
        let parts = result.label.components(separatedBy: " · ")
        XCTAssertGreaterThanOrEqual(parts.count, 3, "\(name): «\(result.label)»")
        let layers = Int(parts.last?.components(separatedBy: " ").first ?? "") ?? 0
        XCTAssertGreaterThan(layers, 0, "\(name): layers in «\(result.label)»")
        let metres = Double(parts.first { $0.hasSuffix(" m") }?.dropLast(2) ?? "") ?? 0
        XCTAssertGreaterThan(metres, 0, "\(name): filament in «\(result.label)»")
        XCTAssertTrue(numbers.first ?? 0 > 0, "\(name): time in «\(result.label)»")
    }

    /// G10: the printer saved on the Printer screen, then «Send to the printer and start»; the simulator gets it and prints.
    private func send(kind: String, port: Int, user: String, secret: String) {
        app.tabBars.buttons["Printer"].tap()
        let picker = app.buttons["printer_kind"]
        XCTAssertTrue(picker.waitForExistence(timeout: 10), "the Printer screen")
        picker.tap()
        let title = ["moonraker": "Klipper (Moonraker)", "octoprint": "OctoPrint", "prusalink": "PrusaLink", "duet": "Duet"][kind]!
        app.buttons[title].firstMatch.tap()
        type(app.textFields["printer_address"], "http://127.0.0.1:\(port)")
        if !user.isEmpty { type(app.textFields["printer_user"], user) }
        type(app.secureTextFields["printer_secret"], secret)
        app.buttons["printer_test"].tap()
        XCTAssertTrue(app.staticTexts["printer_tested"].waitForExistence(timeout: 30), "\(kind) answers the test")
        app.tabBars.buttons["Plate"].tap()
        let send = app.buttons["plate_send"]
        scrollTo(send)
        XCTAssertTrue(send.isEnabled, "«Send to the printer and start» for \(kind)")
        send.tap()
        XCTAssertTrue(wait(120) { self.started(port) }, "\(kind) got the G-code and started (it says \(received(port)))")
    }

    private func received(_ port: Int) -> String {
        guard let url = URL(string: "http://127.0.0.1:\(port)/_tisma/received"), let data = try? Data(contentsOf: url) else { return "nothing" }
        return String(decoding: data, as: UTF8.self)
    }

    private func started(_ port: Int) -> Bool {
        guard let o = try? JSONSerialization.jsonObject(with: Data(received(port).utf8)) as? [String: Any],
              let files = o["files"] as? [String: Any], let state = o["state"] as? String else { return false }
        return !files.isEmpty && (state == "printing" || state == "complete")
    }

    private func type(_ field: XCUIElement, _ text: String) {
        XCTAssertTrue(field.waitForExistence(timeout: 10), "the field \(field.identifier)")
        field.tap()
        if let old = field.value as? String, !old.isEmpty, old != field.placeholderValue {
            field.typeText(String(repeating: XCUIKeyboardKey.delete.rawValue, count: old.count))
        }
        if !text.isEmpty { field.typeText(text) }
    }

    private func scrollTo(_ e: XCUIElement) {
        var n = 0
        while !e.isHittable && n < 8 { app.swipeUp(); n += 1 }
    }

    private func wait(_ seconds: TimeInterval, _ ok: @escaping () -> Bool) -> Bool {
        let end = Date().addingTimeInterval(seconds)
        while Date() < end {
            if ok() { return true }
            RunLoop.current.run(until: Date().addingTimeInterval(0.5))
        }
        return ok()
    }

    private func screen() -> String {
        let alerts = app.alerts.allElementsBoundByIndex.map { "alert «\($0.label)»" }
        return (alerts + app.staticTexts.allElementsBoundByIndex.prefix(40).map { $0.label }).joined(separator: " | ")
    }
}
