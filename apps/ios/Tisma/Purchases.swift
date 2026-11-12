import StoreKit
import TismaShared

/// «Restaurar compras» (cloud.md, rule 5c): when the session opens, each active App Store purchase (StoreKit 2's
/// `Transaction.currentEntitlements`) goes to the server, one by one, so a new device with the same store account
/// gets its plan back. The server verifies each with the App Store; the same one twice answers the same. Buying
/// itself waits for the owner's Apple Developer account (apps/ios/PLAN.md).
enum Purchases {
    /// Sends every active purchase; how many the server took. Errors are kept for the next start (503: the store did
    /// not answer), never shown as a failure of the session.
    @discardableResult
    static func restore(token: String) async -> Int {
        guard !token.isEmpty else { return 0 }
        var sent = 0
        for await result in Transaction.currentEntitlements {
            let productId: String
            switch result {
            case .verified(let t), .unverified(let t, _): productId = t.productID
            }
            let jws = result.jwsRepresentation
            let server = Prefs.server
            let ok = await Task.detached { () -> Bool in
                (try? CloudBridge.shared.sendAppStorePurchase(server: server, token: token, productId: productId,
                                                              signedTransaction: jws)) != nil
            }.value
            if ok { sent += 1 }
        }
        return sent
    }
}
