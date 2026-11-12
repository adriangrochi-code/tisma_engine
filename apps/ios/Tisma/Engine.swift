import Foundation

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

    /// Slices [model] into a G-code beside it; the summary's JSON when it worked.
    static func slice(model: URL) -> (Result, gcode: URL, summary: String?) {
        let gcode = model.deletingPathExtension().appendingPathExtension("gcode")
        let summary = model.deletingPathExtension().appendingPathExtension("summary.json")
        // A bare model takes its settings from --config, as on Android (docs/CONFIG.md keys; the rest default).
        let config = model.deletingPathExtension().appendingPathExtension("settings.json")
        try? #"{"layers":{"height":0.2},"support":{"enabled":false}}"#.write(to: config, atomically: true, encoding: .utf8)
        let r = run(["slice", model.path, "--config", config.path, "-o", gcode.path, "--summary", summary.path, "--no-gpu"])
        return (r, gcode, try? String(contentsOf: summary, encoding: .utf8))
    }
}
