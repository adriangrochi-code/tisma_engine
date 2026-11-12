import Foundation
import os
import TismaShared

/// The engine inside the app (mobile_app.md, rule 1; apps/ios/PLAN.md, step 4): iOS runs no programs, so its command
/// line is called in process through `tisma_run_cli`, with the same arguments as on Android.
enum Engine {
    struct Result {
        let code: Int32
        let out: String
        let err: String
    }

    static func run(_ args: [String]) -> Result {
        let all = ["tisma-engine"] + args
        var owned: [UnsafeMutablePointer<CChar>?] = all.map { strdup($0) }
        defer { owned.forEach { free($0) } }
        var out: UnsafeMutablePointer<CChar>?
        var err: UnsafeMutablePointer<CChar>?
        var outSize = 0
        var errSize = 0
        let code: Int32 = owned.withUnsafeMutableBufferPointer { buf in
            buf.baseAddress!.withMemoryRebound(to: UnsafePointer<CChar>?.self, capacity: buf.count) { argv in
                tisma_run_cli(Int32(all.count), argv, &out, &outSize, &err, &errSize)
            }
        }
        let o = out.map { String(cString: $0) } ?? ""
        let e = err.map { String(cString: $0) } ?? ""
        tisma_free(out)
        tisma_free(err)
        return Result(code: code, out: o, err: e)
    }

    /// The 20 mm calibration cube as an ASCII STL (12 triangles), for the first slice.
    static func cubeStl(size s: Double = 20) -> String {
        let v = [(0.0, 0.0, 0.0), (s, 0, 0), (s, s, 0), (0, s, 0), (0, 0, s), (s, 0, s), (s, s, s), (0, s, s)]
        let faces = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
                     (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
        var t = "solid cube\n"
        for (a, b, c) in faces {
            t += " facet normal 0 0 0\n  outer loop\n"
            for i in [a, b, c] { t += "   vertex \(v[i].0) \(v[i].1) \(v[i].2)\n" }
            t += "  endloop\n endfacet\n"
        }
        return t + "endsolid cube\n"
    }

    /// The catalog the app carries (the engine's printers and filaments).
    static let catalog: Catalog? = {
        func text(_ name: String) -> String? {
            Bundle.main.url(forResource: name, withExtension: "json").flatMap { try? String(contentsOf: $0, encoding: .utf8) }
        }
        guard let p = text("printers"), let f = text("filaments") else { return nil }
        return EngineBridge.shared.catalog(printersJson: p, filamentsJson: f)
    }()

    /// The engine's settings for a printer of the catalog, its nozzle and a filament (`tisma-engine profile`), as a
    /// file for --config; nil (and why) when the engine cannot make them.
    static func profile(printer: String, nozzle: Double, filament: String?) -> (URL?, String) {
        let out = FileManager.default.temporaryDirectory.appendingPathComponent("profile-\(UUID().uuidString).json")
        let r = run(EngineBridge.shared.profileArgs(printer: printer, nozzle: nozzle, filament: filament, out: out.path))
        return (r.code == 0 && FileManager.default.fileExists(atPath: out.path)) ? (out, r.err) : (nil, r.err)
    }

    /// Slices [model] into a G-code beside it with [config] (a profile), or the minimal settings without one; the
    /// summary's JSON when it worked.
    static func slice(model: URL, config profile: URL? = nil) -> (Result, gcode: URL, summary: String?) {
        let gcode = model.deletingPathExtension().appendingPathExtension("gcode")
        let summary = model.deletingPathExtension().appendingPathExtension("summary.json")
        // A bare model takes its settings from --config, as on Android (docs/CONFIG.md keys; the rest default).
        let config = profile ?? model.deletingPathExtension().appendingPathExtension("settings.json")
        if profile == nil {
            try? #"{"layers":{"height":0.2},"support":{"enabled":false}}"#.write(to: config, atomically: true, encoding: .utf8)
        }
        let r = run(["slice", model.path, "--config", config.path, "-o", gcode.path, "--summary", summary.path])
        return (r, gcode, try? String(contentsOf: summary, encoding: .utf8))
    }

    /// A slice as a job of the engine's library (tisma_engine.h): its progress, a way to cancel it, and the memory it
    /// needs checked first (mobile_app.md, rules 12 and 19). One job per object. The job functions are looked up when
    /// the app runs: an engine library without them (the iOS build exports the command line only, for now) makes
    /// every job `refused`, and the plate slices through the command line as before.
    final class SliceJob: @unchecked Sendable {
        enum Outcome {
            case done(summary: String)
            case cancelled
            /// More memory than the phone has left for the app: the cloud is offered.
            case tooLarge(needed: UInt64, available: UInt64)
            case failed(String)
            /// The library refused before slicing, or has no job functions (what the command line may still do).
            case refused(String)
        }

        private typealias Create = @convention(c) () -> OpaquePointer?
        private typealias Destroy = @convention(c) (OpaquePointer?) -> Void
        private typealias WithPath = @convention(c) (OpaquePointer?, UnsafePointer<CChar>?) -> tisma_status
        private typealias Start = @convention(c) (OpaquePointer?) -> tisma_status
        private typealias Poll = @convention(c) (OpaquePointer?) -> tisma_progress
        private typealias Estimate = @convention(c) (OpaquePointer?) -> UInt64
        private typealias Text = @convention(c) (OpaquePointer?) -> UnsafePointer<CChar>?

        private struct Api {
            let create: Create, destroy: Destroy, cancel: Destroy, open: WithPath, settings: WithPath, estimate: Estimate
            let start: Start, poll: Poll, write: WithPath, summary: Text
        }

        private static let api: Api? = {
            let h = UnsafeMutableRawPointer(bitPattern: -2) // RTLD_DEFAULT
            func f<T>(_ name: String, _: T.Type) -> T? { dlsym(h, name).map { unsafeBitCast($0, to: T.self) } }
            guard let create = f("tisma_create", Create.self), let destroy = f("tisma_destroy", Destroy.self),
                  let cancel = f("tisma_cancel", Destroy.self), let open = f("tisma_open_path", WithPath.self),
                  let settings = f("tisma_set_settings_json", WithPath.self), let estimate = f("tisma_estimate_memory", Estimate.self),
                  let start = f("tisma_slice_start", Start.self), let poll = f("tisma_poll", Poll.self),
                  let write = f("tisma_write_gcode", WithPath.self), let summary = f("tisma_summary_json", Text.self) else { return nil }
            return Api(create: create, destroy: destroy, cancel: cancel, open: open, settings: settings, estimate: estimate,
                       start: start, poll: poll, write: write, summary: summary)
        }()

        /// Whether this engine library has the job functions.
        static var available: Bool { api != nil }

        private let ctx: OpaquePointer?
        init() { ctx = Self.api?.create() }
        deinit { if let api = Self.api { api.destroy(ctx) } }

        func cancel() { Self.api?.cancel(ctx) }

        private static func text(_ s: tisma_status) -> String { s.message.map { String(cString: $0) } ?? "code \(s.code.rawValue)" }

        func run(model: URL, settings: String, gcode: URL, progress: @escaping (Int, String) -> Void) -> Outcome {
            guard let api = Self.api, ctx != nil else { return .refused("this engine library has no job functions") }
            var s = api.open(ctx, model.path)
            guard s.code == TISMA_OK else { return .refused(Self.text(s)) }
            s = api.settings(ctx, settings)
            guard s.code == TISMA_OK else { return .refused(Self.text(s)) }
            let needed = api.estimate(ctx)
            let available = UInt64(os_proc_available_memory())
            if available > 0 && needed > available { return .tooLarge(needed: needed, available: available) }
            s = api.start(ctx)
            guard s.code == TISMA_OK else { return .refused(Self.text(s)) }
            while true {
                let p = api.poll(ctx)
                if p.state == TISMA_DONE {
                    let w = api.write(ctx, gcode.path)
                    guard w.code == TISMA_OK else { return .failed(Self.text(w)) }
                    return .done(summary: api.summary(ctx).map { String(cString: $0) } ?? "{}")
                }
                if p.state == TISMA_STOPPED { return .cancelled }
                if p.state == TISMA_FAILED { return .failed(Self.text(p.error)) }
                progress(Int(p.percent), p.stage.map { String(cString: $0) } ?? "")
                Thread.sleep(forTimeInterval: 0.1)
            }
        }
    }
}
