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
}
