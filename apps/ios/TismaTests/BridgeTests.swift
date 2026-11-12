import XCTest
import TismaShared

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
}
