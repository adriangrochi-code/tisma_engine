#pragma once

#include "mesh/mesh.hpp"
#include "project/engineering.hpp"

#include <array>
#include <functional>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

// The engine as the program of Tisma Cloud's nodes (cloud.md, rules 18 and
// 20): a whole project sliced without the interface and without the user's
// data folder, its progress on the standard output and a summary in JSON.
// Own design; see PROVENANCE.md.
namespace tisma {

struct ProjectModel;

// A project's part where it is on the bed: its mesh turned and moved by its
// build transform, its Engineering conditions moved with it, and the way
// back (rows of the inverse turn, then the shift) for a report's points.
struct PlacedPart {
    TriangleMesh mesh;
    ProjectEngineering engineering;
    std::array<double, 12> to_part{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
};
PlacedPart placed_part(const ProjectModel& model);

// Whether `slice` was given the nodes' way: with --summary, --sandbox or
// --no-gpu, or with a project file and no --config.
bool node_slice_wanted(std::span<const std::string_view> args);

// tisma-engine slice <project> -o <out.gcode> --summary <summary.json>
//                    [--sandbox] [--no-gpu] [--yes]
// - the project as the interface saved it: its settings, profiles, models,
//   placement, own settings, volumes, ranges, painting and mixed colours;
// - the G-code as text or binary, as its settings say (gcode.binary);
// - on the output, one line `progress <0 to 100>` each time the progress
//   changes, and nothing else; warnings go to the error output;
// - the summary: print_time ("1h 2m 3s"), print_time_s, filament_mm,
//   filament_g (only when the filament's density is known) and layers; on
//   a failure, `error` with the text for the person and an exit code other
//   than 0 (the summary that cannot be written: the error goes to the error
//   output);
// - --sandbox: the settings' post-processing scripts do not run; without it
//   they run once confirmed (--yes, or Y on the input), as `slice` asks.
int cmd_slice_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

// Whether `analyze` was given the nodes' way (cloud.md, rule 19): with
// --summary, --sandbox, --mode, --object or --instance.
bool node_analyze_wanted(std::span<const std::string_view> args);

// tisma-engine analyze <project> -o <result.tea> --summary <summary.json>
//   --sandbox --mode <analyze|lightest-infill|resin-structure>
//   --quality <fast|normal|high|ultra> --object <n> --instance <n>
//   [--solid] [--no-zones] [--ribs] [--no-gpu]
// - the part is the project's object n (counted from 0, as the project
//   lists them) at its instance n (each object has one placement: 0);
//   its supports, loads and material come from the project;
// - the result .tea: a JSON document of the engine, {format
//   "tisma-engineering-result", version 1, kind "structural", object,
//   instance, fingerprint, report}, the report as the Structural tab keeps
//   it, opened in Engineering as a local analysis;
// - the summary: verdict, min_safety, required_safety,
//   max_displacement_mm, mass_g, family, voxels; or `error`; progress and
//   exit codes as for slice;
// - only mode "analyze" runs yet: the others answer with `error` and exit
//   code 3 (not implemented).
int cmd_analyze_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

// tisma-engine aero <project> -o <result.taero> --summary <summary.json>
//   --sandbox --object <n> --instance <n> --quality <friction|fast|normal|high>
//   --speed <m/s> --direction <x|-x|y|-y|z|-z|x,y,z> [--no-gpu]
// (cloud.md, rule 19b; aerodynamics.md): the part where it is on the bed,
// the layer height from the project; the result .taero is the analysis'
// JSON (aero::result_json) with the object, instance and part; the summary
// drag_n, cd, cd_pressure, cd_friction, reynolds and reynolds_simulated, or
// `error`. Progress and exit codes as for slice.
int cmd_aero_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

// tisma-engine orient <project> -o <orientation.json> --summary <summary.json>
//   --sandbox --object <n> --instance <n> [--threads <n>] [--no-gpu]
// (cloud.md, rules 10c and 19c; orientation.md, rules 1 to 6, «menos material
// de soporte»): the part where it is on the bed; the result is JSON with
// "rotation" (3 x 3 by rows, in the bed's axes, applied after the part's
// placement), "cost" and "cost_before"; the summary cost, cost_before and
// candidates, or `error` and `code`.
int cmd_orient_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

// Writes a file in one piece: into "<path>.part" first, renamed over the
// path once complete, so a process stopped half way never leaves a partial
// file under the real name. False (and no temporary left) on failure.
bool write_file(const std::string& path, const std::function<void(std::ostream&)>& write);

} // namespace tisma
