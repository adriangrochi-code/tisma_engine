import Foundation

/// Opening a model from a model site's link (mobile_app.md, rule 10; parity P9), with Android's rules:
/// `tisma://open?file=<encoded https URL>` or `prusaslicer://open?file=…` (Printables' «Open in PrusaSlicer»). Only
/// https, only the listed sites (also after redirects), at most 200 MB, only STL, OBJ or 3MF (the engine reads no STEP;
/// a ZIP waits for an unzip on the iPhone). Only when the person allows it (Preferences). Own design.
enum SiteLink {
    static let schemes: Set<String> = ["tisma", "prusaslicer"]
    static let hosts: Set<String> = ["printables.com", "media.printables.com", "files.printables.com", "thingiverse.com", "cdn.thingiverse.com"]
    static let extensions: Set<String> = ["stl", "obj", "3mf"]
    static let maxBytes = 200 * 1024 * 1024

    struct Request: Identifiable, Equatable {
        let url: URL
        let fileName: String
        let site: String
        var id: String { url.absoluteString }
    }

    enum Reason: Error, Equatable { case notALink, notHttps, site, kind, tooLarge, failed(String) }

    static func allowed(host: String) -> Bool {
        let h = host.lowercased()
        return hosts.contains(h.hasPrefix("www.") ? String(h.dropFirst(4)) : h)
    }

    static func safeName(_ name: String) -> String {
        let s = String(name.map { $0.isLetter && $0.isASCII || $0.isNumber && $0.isASCII || "._ -".contains($0) ? $0 : "_" })
        return s.trimmingCharacters(in: .whitespaces).isEmpty ? "model.stl" : s
    }

    /// The request a link makes, or why it is refused.
    static func parse(_ link: URL) -> Result<Request, Reason> {
        guard let c = URLComponents(url: link, resolvingAgainstBaseURL: false), let scheme = c.scheme?.lowercased(),
              schemes.contains(scheme), c.host?.lowercased() == "open",
              let file = c.queryItems?.first(where: { $0.name == "file" })?.value, let target = URL(string: file) else {
            return .failure(.notALink)
        }
        guard target.scheme?.lowercased() == "https" else { return .failure(.notHttps) }
        guard let host = target.host, allowed(host: host) else { return .failure(.site) }
        let name = target.lastPathComponent
        guard extensions.contains((name as NSString).pathExtension.lowercased()) else { return .failure(.kind) }
        let site = host.lowercased().hasPrefix("www.") ? String(host.lowercased().dropFirst(4)) : host.lowercased()
        return .success(Request(url: target, fileName: safeName(name), site: site))
    }

    /// Refuses a redirect off https or off the listed sites.
    private final class Guard: NSObject, URLSessionTaskDelegate {
        func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                        newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) {
            let ok = request.url?.scheme?.lowercased() == "https" && request.url?.host.map { SiteLink.allowed(host: $0) } == true
            completionHandler(ok ? request : nil)
        }
    }

    /// Downloads the model into the app's models folder; its file.
    static func download(_ r: Request) async throws -> URL {
        let session = URLSession(configuration: .ephemeral, delegate: Guard(), delegateQueue: nil)
        defer { session.finishTasksAndInvalidate() }
        let (tmp, response) = try await session.download(from: r.url)
        guard let http = response as? HTTPURLResponse, let final = http.url else { throw Reason.failed("no answer") }
        guard final.scheme?.lowercased() == "https", let h = final.host, allowed(host: h) else { throw Reason.site }
        guard (200...299).contains(http.statusCode) else { throw Reason.failed("HTTP \(http.statusCode)") }
        let size = (try? FileManager.default.attributesOfItem(atPath: tmp.path)[.size] as? Int) ?? 0
        guard size <= maxBytes else { throw Reason.tooLarge }
        let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("models")
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        let out = dir.appendingPathComponent(r.fileName)
        try? FileManager.default.removeItem(at: out)
        try FileManager.default.moveItem(at: tmp, to: out)
        return out
    }

    static func text(_ reason: Reason) -> String {
        switch reason {
        case .notALink: return String(localized: "The link cannot be read.")
        case .notHttps: return String(localized: "The link is not secure (https): nothing is downloaded.")
        case .site: return String(localized: "That site is not among the supported ones (Printables and Thingiverse).")
        case .kind: return String(localized: "Only STL, OBJ and 3MF open on the iPhone.")
        case .tooLarge: return String(localized: "The model is larger than 200 MB.")
        case .failed(let why): return String(format: String(localized: "The model could not be downloaded: %@"), why)
        }
    }
}
