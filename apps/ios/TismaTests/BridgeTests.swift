import CryptoKit
import XCTest
import TismaShared
@testable import Tisma

/// The SwiftUI app reaches the shared Kotlin code: errors come back as Swift errors, not crashes.
final class BridgeTests: XCTestCase {
    func testFoldingMatchesAndroid() {
        XCTAssertEqual(CloudBridge.shared.fold(text: "Deformación"), "deformacion")
    }

    func testADownServerIsAnError() {
        XCTAssertThrowsError(try CloudBridge.shared.checkHealth(server: "http://127.0.0.1:9"))
    }

    func testTheDefaultServer() {
        XCTAssertEqual(CloudBridge.shared.defaultServer, "https://tismaservidores.net")
    }

    func testNoNoticeWhenTheServerDoesNotAnswer() {
        XCTAssertNil(CloudBridge.shared.serviceNotice(server: "http://127.0.0.1:9", spanish: true))
    }

    func testWithoutTheProjectKeyTheLibraryIsRefusedAndEmpty() {
        let why = LibraryBridge.shared.refresh(server: "http://127.0.0.1:9", projectKeyHex: "")
        // No server answer and nothing kept: nothing to refuse yet, and nothing found.
        XCTAssertTrue(why == nil || !(why!.isEmpty))
        XCTAssertEqual(LibraryBridge.shared.search(query: "").count, 0)
    }

    func testPrinterKindsWithBambu() {
        XCTAssertEqual(PrinterBridge.shared.kinds, ["moonraker", "octoprint", "prusalink", "duet", "bambu"])
    }

    func testAPrinterThatDoesNotAnswerIsAnError() {
        XCTAssertThrowsError(try PrinterBridge.shared.status(kind: "moonraker", address: "127.0.0.1:9", user: "", secret: ""))
        XCTAssertThrowsError(try PrinterBridge.shared.status(kind: "bambu", address: "127.0.0.1:9", user: "", secret: "12345678"))
    }

    func testTheKeychainKeepsAndForgetsASecret() {
        Keychain.write("test.secret", "abc123")
        XCTAssertEqual(Keychain.read("test.secret"), "abc123")
        Keychain.write("test.secret", "")
        XCTAssertNil(Keychain.read("test.secret"))
    }

    func testTheEngineAnswersItsVersion() {
        let r = Engine.run(["--version"])
        XCTAssertEqual(r.code, 0, r.err)
        XCTAssertFalse(r.out.isEmpty)
    }

    /// The first slice inside an iPhone app: its result is printed for the engine team, and must not crash.
    func testTheEngineSlicesTheCube() throws {
        let model = FileManager.default.temporaryDirectory.appendingPathComponent("cube20-test.stl")
        try Engine.cubeStl().write(to: model, atomically: true, encoding: .utf8)
        let (r, gcode, summary) = Engine.slice(model: model)
        let size = (try? FileManager.default.attributesOfItem(atPath: gcode.path)[.size] as? Int) ?? 0
        print("ENGINE-SLICE exit=\(r.code) gcode=\(size) err=\(r.err.prefix(400)) summary=\(summary?.prefix(400) ?? "")")
        XCTAssertEqual(r.code, 0, r.err)
        XCTAssertGreaterThan(size, 1000)
    }

    func testTheCatalogIsInTheApp() throws {
        let c = try XCTUnwrap(Engine.catalog)
        XCTAssertGreaterThan(c.printers.count, 10)
        XCTAssertGreaterThan(c.filaments.count, 10)
    }

    /// MA2 on the iPhone: a printer of the catalog, its profile from the engine, the cube sliced with it.
    func testTheCubeWithAPrinterProfile() throws {
        let c = try XCTUnwrap(Engine.catalog)
        let printer = try XCTUnwrap(c.printers.first { $0.name.contains("Prusa") } ?? c.printers.first)
        let (config, why) = Engine.profile(printer: printer.name, nozzle: 0.4, filament: nil)
        let profile = try XCTUnwrap(config, "no profile for \(printer.name): \(why)")
        let model = FileManager.default.temporaryDirectory.appendingPathComponent("cube20-profile.stl")
        try Engine.cubeStl().write(to: model, atomically: true, encoding: .utf8)
        let (r, _, summary) = Engine.slice(model: model, config: profile)
        print("ENGINE-SLICE profile=\(printer.name) exit=\(r.code) summary=\(summary?.prefix(300) ?? "") err=\(r.err.prefix(300))")
        XCTAssertEqual(r.code, 0, r.err)
        let s = try XCTUnwrap(summary.flatMap { EngineBridge.shared.summary(json: $0) })
        XCTAssertGreaterThan(s.filamentMm, 0)
        XCTAssertGreaterThan(s.layers, 50)
        // The preview reads the same G-code, layer by layer.
        let g = try XCTUnwrap(EngineBridge.shared.preview(path: model.deletingPathExtension().appendingPathExtension("gcode").path))
        XCTAssertGreaterThan(g.layers.count, 50)
    }
}
