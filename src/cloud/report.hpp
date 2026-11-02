#pragma once

// What a problem report sends (docs/spec/features/problem_reports.md, rules 2
// and 3; cloud.md, rule 11): the person's words and, if they accept, the
// version, the system and the session's log with the printer and the
// profiles on top. Own design; see PROVENANCE.md.

#include <map>
#include <string>

namespace tisma::cloud {

struct ReportInput {
    std::string kind = "bug"; // bug, crash or suggestion
    std::string title, description, steps, expected;
    bool attach = true;
    std::string app_version, os;
    std::string printer, print_profile, material_profile; // the printer in use and its profiles
    std::string session_log;
};

inline constexpr std::size_t kReportTextMax = 10000;  // description, steps, expected
inline constexpr std::size_t kReportLogMax = 190000;  // the log: its end when longer
inline constexpr std::size_t kReportOsMax = 80;

// The log as it is sent (and as «See what is attached» shows it): the program,
// the system, the printer and its profiles, then the session's log.
std::string report_log(const ReportInput& in);

// The fields of POST /v1/reports. Without `attach`, no app_version, os nor log.
std::map<std::string, std::string> report_fields(const ReportInput& in);

} // namespace tisma::cloud
