#include "cloud/report.hpp"

namespace tisma::cloud {

namespace {

// At most `max` bytes, cut at a UTF-8 character's start.
std::string head(const std::string& text, std::size_t max) {
    if (text.size() <= max)
        return text;
    std::size_t end = max;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)
        --end;
    return text.substr(0, end);
}

// The last `max` bytes at most, from a UTF-8 character's start.
std::string tail(const std::string& text, std::size_t max) {
    if (text.size() <= max)
        return text;
    std::size_t start = text.size() - max;
    while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80)
        ++start;
    return text.substr(start);
}

} // namespace

std::string report_log(const ReportInput& in) {
    std::string top = "Tisma " + in.app_version + "\n" + "System: " + in.os + "\n" + "Printer: " +
                      (in.printer.empty() ? std::string("-") : in.printer) + "\n" + "Print profile: " +
                      (in.print_profile.empty() ? std::string("-") : in.print_profile) + "\n" + "Material profile: " +
                      (in.material_profile.empty() ? std::string("-") : in.material_profile) + "\n\n";
    if (top.size() >= kReportLogMax)
        return head(top, kReportLogMax);
    return top + tail(in.session_log, kReportLogMax - top.size());
}

std::map<std::string, std::string> report_fields(const ReportInput& in) {
    std::map<std::string, std::string> f;
    f["app"] = "pc";
    f["kind"] = in.kind;
    f["title"] = head(in.title, 200);
    f["description"] = head(in.description, kReportTextMax);
    f["steps"] = head(in.steps, kReportTextMax);
    f["expected"] = head(in.expected, kReportTextMax);
    if (in.attach) {
        f["app_version"] = in.app_version;
        f["os"] = head(in.os, kReportOsMax);
        f["log"] = report_log(in);
    }
    return f;
}

} // namespace tisma::cloud
