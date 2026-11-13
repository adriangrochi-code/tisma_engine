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
        // A printer of the installed profile pack (chosen in the assistant): its variant with the pack's own default
        // filament and print profile (profile_pack.md, rule 13; parity P10b), as on Android.
        let pack = UserDefaults.standard.string(forKey: "plate.pack") ?? ""
        let args = !pack.isEmpty && Pack.installed
            ? PackBridge.shared.profileArgs(packDir: Pack.packDir.path, printer: pack, out: out.path,
                                            filament: UserDefaults.standard.string(forKey: "plate.packFilament") ?? "")
            : EngineBridge.shared.profileArgs(printer: printer, nozzle: nozzle, filament: filament, out: out.path)
        let r = run(args)
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

    /// A slice as a job of the engine's library (tisma_engine.h, docs/C_API.md): its progress by stage, a way to cancel
    /// it, and the memory it needs checked first (mobile_app.md, rules 12 and 19). One job per object.
    final class SliceJob: @unchecked Sendable {
        enum Outcome {
            case done(summary: String)
            case cancelled
            /// More memory than the phone has left for the app: the cloud is offered.
            case tooLarge(needed: UInt64, available: UInt64)
            case failed(String)
            /// The library refused before slicing (what the command line may still do).
            case refused(String)
        }

        private let ctx = tisma_create()
        deinit { tisma_destroy(ctx) }

        /// Stops at the next stage (0.05 to 0.5 s on a computer).
        func cancel() { tisma_cancel(ctx) }

        private static func text(_ s: tisma_status) -> String { s.message.map { String(cString: $0) } ?? "code \(s.code.rawValue)" }

        func run(model: URL, settings: String, gcode: URL, progress: @escaping (Int, String) -> Void) -> Outcome {
            guard ctx != nil else { return .refused("the engine has no context") }
            var s = tisma_open_path(ctx, model.path)
            guard s.code == TISMA_OK else { return .refused(Self.text(s)) }
            s = tisma_set_settings_json(ctx, settings)
            guard s.code == TISMA_OK else { return .refused(Self.text(s)) }
            let needed = tisma_estimate_memory(ctx)
            let available = UInt64(os_proc_available_memory())
            if available > 0 && needed > available { return .tooLarge(needed: needed, available: available) }
            s = tisma_slice_start(ctx)
            guard s.code == TISMA_OK else { return .refused(Self.text(s)) }
            while true {
                let p = tisma_poll(ctx)
                if p.state == TISMA_DONE {
                    let w = tisma_write_gcode(ctx, gcode.path)
                    guard w.code == TISMA_OK else { return .failed(Self.text(w)) }
                    return .done(summary: tisma_summary_json(ctx).map { String(cString: $0) } ?? "{}")
                }
                if p.state == TISMA_STOPPED { return .cancelled }
                if p.state == TISMA_FAILED { return .failed(Self.text(p.error)) }
                progress(Int(p.percent), p.stage.map { String(cString: $0) } ?? "")
                Thread.sleep(forTimeInterval: 0.1)
            }
        }
    }
}
