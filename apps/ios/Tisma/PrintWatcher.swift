import BackgroundTasks
import SwiftUI
import TismaShared
import UserNotifications

/// Follows the saved printer and tells the person when a print finishes, stops, pauses or fails (docs/MOBILE.md,
/// «Avisos»; the same rules as Android's watcher, from the shared PrintEvents). While the app is open it reads the
/// printer every 5 s; in the background iOS gives no steady timer, so an app refresh task reads it when the system
/// allows (best effort: the notice can come late, or not at all if the app was closed by hand).
@MainActor
final class PrintWatcher: ObservableObject {
    static let shared = PrintWatcher()
    static let refreshTask = "net.tismaservidores.tisma.watch"

    @Published private(set) var line: PrinterLine?
    @Published private(set) var problem: String?

    private var loop: Task<Void, Never>?

    /// The printer as saved by the Printer screen (address and kind in the defaults, the key in the Keychain).
    nonisolated static func saved() -> (String, String, String, String)? {
        let d = UserDefaults.standard
        let address = d.string(forKey: "printer.address") ?? ""
        guard !address.isEmpty else { return nil }
        return (d.string(forKey: "printer.kind") ?? "moonraker", address, d.string(forKey: "printer.user") ?? "",
                Keychain.read("printer.secret") ?? "")
    }

    /// Reads the printer every 5 s while the app is active.
    func start() {
        guard loop == nil else { return }
        loop = Task { [weak self] in
            while !Task.isCancelled {
                await self?.readOnce()
                try? await Task.sleep(for: .seconds(5))
            }
        }
    }

    func stop() { loop?.cancel(); loop = nil }

    /// One reading; a notice if something happened since the last.
    func readOnce() async {
        guard let a = Self.saved() else { line = nil; problem = nil; return }
        let result: Result<PrinterLine, Error> = await Task.detached {
            Result { try PrinterBridge.shared.status(kind: a.0, address: a.1, user: a.2, secret: a.3) }
        }.value
        switch result {
        case .success(let l):
            line = l; problem = nil
            if l.state == "printing" || l.state == "paused" { Self.askToNotify() }
            if !l.event.isEmpty { Self.notify(l, printer: PrinterKinds.title(a.0)) }
        case .failure(let e):
            problem = e.localizedDescription
        }
    }

    var printing: Bool { line?.state == "printing" || line?.state == "paused" }

    // MARK: notices

    /// Asked once, when a print is first seen (not at the first start: the person knows then why).
    static func askToNotify() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    /// The notice's title and body; the same words as Android's (event_finished… in both languages).
    nonisolated static func text(_ l: PrinterLine, printer: String) -> (String, String) {
        let title: String
        switch l.event {
        case "finished": title = String(format: String(localized: "%@ finished printing"), printer)
        case "stopped": title = String(format: String(localized: "%@ stopped printing"), printer)
        case "paused": title = String(format: String(localized: "%@ paused"), printer)
        default: title = String(format: String(localized: "%@ has an error"), printer)
        }
        return (title, l.eventDetail)
    }

    nonisolated static func notify(_ l: PrinterLine, printer: String) {
        let (title, body) = text(l, printer: printer)
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        content.sound = .default
        UNUserNotificationCenter.current().add(UNNotificationRequest(identifier: "print-\(l.event)-\(Date().timeIntervalSince1970)",
                                                                     content: content, trigger: nil))
    }

    // MARK: background

    /// Registered at start (before the app finishes launching).
    nonisolated static func registerBackground() {
        BGTaskScheduler.shared.register(forTaskWithIdentifier: refreshTask, using: nil) { task in
            guard let task = task as? BGAppRefreshTask else { return }
            scheduleBackground()
            let work = Task.detached {
                if let a = saved(), let l = try? PrinterBridge.shared.status(kind: a.0, address: a.1, user: a.2, secret: a.3),
                   !l.event.isEmpty {
                    notify(l, printer: PrinterKinds.title(a.0))
                }
                task.setTaskCompleted(success: true)
            }
            task.expirationHandler = { work.cancel() }
        }
    }

    /// Asks iOS for a reading in about 5 minutes (it decides when; only while a print is going).
    nonisolated static func scheduleBackground() {
        let request = BGAppRefreshTaskRequest(identifier: refreshTask)
        request.earliestBeginDate = Date(timeIntervalSinceNow: 5 * 60)
        try? BGTaskScheduler.shared.submit(request)
    }
}

/// Shows the notices while the app is open, too.
final class NoticeDelegate: NSObject, UNUserNotificationCenterDelegate {
    static let shared = NoticeDelegate()

    func userNotificationCenter(_ center: UNUserNotificationCenter, willPresent notification: UNNotification,
                                withCompletionHandler done: @escaping (UNNotificationPresentationOptions) -> Void) {
        done([.banner, .sound])
    }
}
