#pragma once

#include <filesystem>
#include <optional>
#include <string>

// A session of the interface leaves a mark while it runs, so the next start
// knows when one closed without its normal exit (problem_reports.md, rule 10).
// In the folder of the preferences:
// - session-<process id>.lock while it runs, removed at a normal exit;
// - session-<process id>.log, its log, line by line, removed with the mark.
// A mark whose process is gone is a session that closed unexpectedly. Own
// design; see PROVENANCE.md.
namespace tisma::ui {

long current_process_id();
// Whether a process with this id runs now (a reused id counts as running:
// the worst it does is not asking about that closing).
bool process_running(long id);

struct SessionMark {
    std::filesystem::path lock, log; // empty when the folder cannot be written
};
SessionMark mark_session(const std::filesystem::path& folder);
void end_session(const SessionMark& mark);

// The log of the newest session in `folder` that closed unexpectedly, its
// last `max_bytes`, or none. Every mark of a gone process is removed with its
// log, so the same closing is never asked about twice.
std::optional<std::string> closed_session_log(const std::filesystem::path& folder, std::size_t max_bytes = 190000);

} // namespace tisma::ui
