#pragma once

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
//   filament_g and layers; on a failure, `error` with the text for the
//   person and an exit code other than 0 (the summary that cannot be
//   written: the error goes to the error output);
// - --sandbox: the settings' post-processing scripts do not run; without it
//   they run once confirmed (--yes, or Y on the input), as `slice` asks.
int cmd_slice_project(std::span<const std::string_view> args, std::ostream& out, std::ostream& err);

// Writes a file in one piece: into "<path>.part" first, renamed over the
// path once complete, so a process stopped half way never leaves a partial
// file under the real name. False (and no temporary left) on failure.
bool write_file(const std::string& path, const std::function<void(std::ostream&)>& write);

} // namespace tisma
