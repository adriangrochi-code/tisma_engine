import Foundation
import TismaShared

/// Searches the local network with the system's Bonjour (DNS-SD) while the Printer screen is open (printer_connections.md,
/// «Búsqueda en la red»; parity P17): OctoPrint, Moonraker and plain web services, the same types and rules as
/// Android, kept in the shared list. Finding never connects: the person picks one to fill in the address.
@MainActor
final class PrinterDiscovery: NSObject, ObservableObject {
    @Published private(set) var found: [FoundLine] = []

    private var browsers: [NetServiceBrowser] = []
    private var resolving: [NetService] = []
    private var types: [ObjectIdentifier: String] = [:]
    private var saved = ""

    func start(saved: String) {
        self.saved = saved
        guard browsers.isEmpty else { refresh(); return }
        for type in PrinterBridge.shared.discoveryTypes {
            let b = NetServiceBrowser()
            b.delegate = self
            types[ObjectIdentifier(b)] = type
            b.searchForServices(ofType: type + ".", inDomain: "local.")
            browsers.append(b)
        }
    }

    func stop() {
        browsers.forEach { $0.stop() }
        browsers.removeAll()
        resolving.forEach { $0.stop() }
        resolving.removeAll()
    }

    private func refresh() { found = PrinterBridge.shared.found(savedAddress: saved) }

    fileprivate func resolved(_ s: NetService) {
        resolving.removeAll { $0 === s }
        guard let host = s.hostName?.trimmingCharacters(in: CharacterSet(charactersIn: ".")), s.port > 0 else { return }
        let txt = s.txtRecordData().map { NetService.dictionary(fromTXTRecord: $0) } ?? [:]
        func t(_ k: String) -> String { txt[k].flatMap { String(data: $0, encoding: .utf8) } ?? "" }
        let model = [t("vendor"), t("model")].filter { !$0.isEmpty }.joined(separator: " ")
        let (type, name, port, path) = (s.type, s.name, Int32(s.port), t("path"))
        Task.detached {
            // A plain web service is asked whether Moonraker answers: off the main thread.
            PrinterBridge.shared.seen(type: type, name: name, host: host, port: port, path: path, model: model)
            await MainActor.run { self.refresh() }
        }
    }

    fileprivate func lost(_ s: NetService) {
        if let host = s.hostName?.trimmingCharacters(in: CharacterSet(charactersIn: ".")) {
            PrinterBridge.shared.lost(host: host, port: Int32(s.port))
        }
        refresh()
    }
}

extension PrinterDiscovery: NetServiceBrowserDelegate, NetServiceDelegate {
    nonisolated func netServiceBrowser(_ browser: NetServiceBrowser, didFind service: NetService, moreComing: Bool) {
        MainActor.assumeIsolated {
            service.delegate = self
            resolving.append(service)
            service.resolve(withTimeout: 5)
        }
    }

    nonisolated func netServiceBrowser(_ browser: NetServiceBrowser, didRemove service: NetService, moreComing: Bool) {
        MainActor.assumeIsolated { lost(service) }
    }

    nonisolated func netServiceDidResolveAddress(_ sender: NetService) {
        MainActor.assumeIsolated { resolved(sender) }
    }

    nonisolated func netService(_ sender: NetService, didNotResolve errorDict: [String: NSNumber]) {
        MainActor.assumeIsolated { resolving.removeAll { $0 === sender } }
    }
}
