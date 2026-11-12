import XCTest

/// Opens each section and keeps a screenshot of it (CLAUDE.md: screenshots of the screens that changed). The CI
/// passes the folder as TEST_RUNNER_SCREENSHOT_DIR; without it the pictures stay in the test result only.
final class ScreensTests: XCTestCase {
    override func setUp() { continueAfterFailure = false }

    func testEachSectionOpens() {
        let app = XCUIApplication()
        app.launchArguments += ["--first-start"]
        app.launch()
        // MA1: the first start opens the assistant; a printer, then its filament, close it.
        XCTAssertTrue(app.navigationBars["Welcome to Tisma"].waitForExistence(timeout: 10))
        keep("wizard")
        app.buttons.matching(identifier: "wizard_printer").firstMatch.tap()
        let nozzle = app.buttons["0.40 mm"]
        if nozzle.waitForExistence(timeout: 2) { nozzle.tap() }
        let filament = app.buttons.matching(identifier: "wizard_filament").firstMatch
        XCTAssertTrue(filament.waitForExistence(timeout: 5))
        filament.tap()
        for (tab, name) in [("Plate", "plate"), ("Print", "print"), ("Material", "material"), ("Printer", "printer"), ("More", "more")] {
            let button = app.tabBars.buttons[tab]
            XCTAssertTrue(button.waitForExistence(timeout: 10), "the \(tab) section is there")
            button.tap()
            if tab == "Plate" {
                // The printer card: no printer saved yet, so its connection icon says none (rule 42).
                XCTAssertTrue(app.buttons["connection_none"].waitForExistence(timeout: 10), "the printer card is on the plate")
                // Engineering, with the part to touch (the cube).
                let engineering = app.buttons["plate_engineering"]
                if !engineering.isHittable { app.swipeUp() }
                engineering.tap()
                XCTAssertTrue(app.navigationBars["Engineering"].waitForExistence(timeout: 10))
                XCTAssertTrue(app.otherElements["engineering_part"].waitForExistence(timeout: 10), "the part in 3D")
                keep("engineering")
                app.navigationBars["Engineering"].buttons.firstMatch.tap()
            }
            keep(name)
        }
        XCTAssertTrue(app.navigationBars["More"].waitForExistence(timeout: 5))
        // Report a problem (parity P6).
        let report = app.buttons["more_report"]
        if !report.isHittable { app.swipeUp() }
        report.tap()
        XCTAssertTrue(app.navigationBars["Report a problem"].waitForExistence(timeout: 5))
        keep("report")
    }

    private func keep(_ name: String) {
        let shot = XCUIScreen.main.screenshot()
        let attachment = XCTAttachment(screenshot: shot)
        attachment.name = name
        attachment.lifetime = .keepAlways
        add(attachment)
        if let dir = ProcessInfo.processInfo.environment["SCREENSHOT_DIR"], !dir.isEmpty {
            try? FileManager.default.createDirectory(atPath: dir, withIntermediateDirectories: true)
            try? shot.pngRepresentation.write(to: URL(fileURLWithPath: dir).appendingPathComponent("ios-\(name).png"))
        }
    }
}
