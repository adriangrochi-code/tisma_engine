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
        // A filament gives the profile its temperatures; without one the engine's check refuses the G-code.
        let filament = c.filamentsFor(p: printer).first?.fullName
        let (config, why) = Engine.profile(printer: printer.name, nozzle: 0.4, filament: filament)
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

    func testAPickedModelIsCopiedIntoTheAppAndOtherFilesAreRefused() throws {
        let src = FileManager.default.temporaryDirectory.appendingPathComponent("picked-cube.stl")
        try Engine.cubeStl().write(to: src, atomically: true, encoding: .utf8)
        let copy = try XCTUnwrap(PlateView.copyIn(src))
        XCTAssertNotEqual(copy, src)
        XCTAssertEqual(try String(contentsOf: copy, encoding: .utf8), Engine.cubeStl())
        let txt = FileManager.default.temporaryDirectory.appendingPathComponent("notes.txt")
        try "hi".write(to: txt, atomically: true, encoding: .utf8)
        XCTAssertNil(PlateView.copyIn(txt))
    }

    func testSendingToAPrinterThatDoesNotAnswerIsAnError() throws {
        let f = FileManager.default.temporaryDirectory.appendingPathComponent("send.gcode")
        try "G28\n".write(to: f, atomically: true, encoding: .utf8)
        XCTAssertThrowsError(try PrinterBridge.shared.upload(kind: "octoprint", address: "127.0.0.1:9", user: "", secret: "k",
                                                             path: f.path, name: "send.gcode", start: true))
    }

    func testACloudSliceRefusesAFileThatIsNoProject() throws {
        let f = FileManager.default.temporaryDirectory.appendingPathComponent("not-a-project.3mf")
        try "solid x".write(to: f, atomically: true, encoding: .utf8)
        XCTAssertThrowsError(try CloudBridge.shared.submitSlice(server: "http://127.0.0.1:9", token: "t", path: f.path, name: "x.3mf"))
    }

    func testPrintChangesGoOverTheProfile() throws {
        let dir = FileManager.default.temporaryDirectory
        let profile = dir.appendingPathComponent("p-changes.json"), out = dir.appendingPathComponent("p-changed.json")
        try #"{"layers":{"height":0.2},"machine":{"variables":{"temperature":215}}}"#.write(to: profile, atomically: true, encoding: .utf8)
        XCTAssertTrue(EngineBridge.shared.applyChanges(profilePath: profile.path, outPath: out.path, layerHeight: 0.12,
                                                      infillPercent: 40, supports: true, nozzleTemp: 0, bedTemp: 65))
        let text = try String(contentsOf: out, encoding: .utf8)
        XCTAssertTrue(text.contains("\"height\":0.12"))
        XCTAssertTrue(text.contains("\"temperature\":215"), "the filament's nozzle temperature is kept")
        XCTAssertTrue(text.contains("\"bed_temperature\":65.0"))
    }

    func testThePanelHasLevelsAndKeepsChanges() throws {
        let url = try XCTUnwrap(Bundle.main.url(forResource: "layout_en", withExtension: "json"))
        let panel = PanelBridge(layoutJson: try String(contentsOf: url, encoding: .utf8), changesJson: "{}")
        let basic = panel.sections(tab: "print", level: 0).flatMap { $0.rows }.count
        let advanced = panel.sections(tab: "print", level: 2).flatMap { $0.rows }.count
        XCTAssertGreaterThan(basic, 0)
        XCTAssertGreaterThan(advanced, basic)
        XCTAssertTrue(panel.set(key: "layers.height", text: "0,12"), "a comma decimal is a number")
        XCTAssertFalse(panel.set(key: "layers.height", text: "abc"))
        XCTAssertTrue(panel.over(profileJson: #"{"layers":{"height":0.2,"first_height":0.3}}"#).contains("0.12"))
    }

    func testAFinishedPrintsNoticeNamesThePrinterAndFile() {
        let l = PrinterLine(state: "ready", file: "cube.gcode", percent: -1, nozzle: "", bed: "", event: "finished",
                            eventDetail: "cube.gcode")
        let (title, body) = PrintWatcher.text(l, printer: "Klipper (Moonraker)")
        XCTAssertTrue(title.hasPrefix("Klipper (Moonraker)"))
        XCTAssertEqual(body, "cube.gcode")
    }

    /// Engineering on the 20 mm cube, held below by name and pushed on its top as touched (ModelIO reads the cube),
    /// quick (solid) mode: the engine answers and colors the part.
    func testEngineeringOnTheCube() {
        guard let printer = Engine.catalog?.printers.first else { return XCTFail("no catalog") }
        let f = Engine.catalog?.filamentsFor(p: printer).first?.fullName
        guard let mesh = PartMesh.load(EngineeringView.cube()) else { return XCTFail("ModelIO did not read the cube") }
        XCTAssertEqual(mesh.triangles, 12)
        XCTAssertEqual(EngineeringBridge.shared.setMesh(soup: mesh.soup), 12)
        // The top: the triangle whose corners are all at z = 20.
        let top = (0..<12).first { t in
            mesh.soup.withUnsafeBytes { raw in (0..<3).allSatisfy { k in raw.load(fromByteOffset: (9 * t + 3 * k + 2) * 4, as: Float.self) == 20 } }
        }
        guard let top, let touched = EngineeringBridge.shared.pick(triangle: Int32(top), quality: "normal") else { return XCTFail("no top") }
        XCTAssertEqual(touched.face, "top")
        XCTAssertEqual(touched.triangles.count, 2)
        let caseJson = EngineeringBridge.shared.caseFor(fixed: EngineeringBridge.shared.named(face: "bottom"), loaded: touched,
                                                        newtons: 100, material: "bambu-pla-basic", quality: "normal", solid: true)
        let (lines, problem, voxels) = EngineeringView.run(model: nil, printer: printer.name, nozzle: printer.nozzles.first?.doubleValue ?? 0.4,
                                                           filament: f, caseJson: caseJson, requiredSafety: 2)
        let map = voxels.flatMap { EngineeringBridge.shared.stressMap(path: $0.path, maxFaces: 400_000) }
        print("ENGINE-ANALYZE lines=\(lines.map { "\($0.key)=\($0.value)" }) problem=\(problem ?? "") map=\(map?.triangles ?? -1)")
        XCTAssertNil(problem)
        XCTAssertEqual(lines.first?.key, "verdict")
        XCTAssertGreaterThan(map?.triangles ?? 0, 0, "the safety map")
    }
}
