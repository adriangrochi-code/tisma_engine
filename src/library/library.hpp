#pragma once

// My library (docs/spec/features/local_library.md): parts kept with their
// project, a recipe per printer they were sliced for and their analyses, in a
// folder per entry, the same on the computer and on the phone. No G-code is
// kept. Own design; see PROVENANCE.md.

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace tisma {

// A request the library cannot do: a code for the caller, and the message in
// English and in Spanish.
class LibraryError : public std::runtime_error {
public:
    LibraryError(std::string code, const std::string& message, std::string message_es)
        : std::runtime_error(message), code_(std::move(code)), es_(std::move(message_es)) {}
    const std::string& code() const { return code_; }
    const std::string& message_es() const { return es_; }

private:
    std::string code_, es_;
};

// The library's entries as a list: id, name, tags, printers, materials,
// recipe and analysis counts, bytes on disk, last change and thumbnail.
nlohmann::json library_list(const std::filesystem::path& dir);

// One entry's entry.json.
nlohmann::json library_show(const std::filesystem::path& dir, const std::string& id);

// What to keep (rule 3): a new entry, or a recipe and an analysis added to an
// entry. A recipe for the same printer, nozzle, filament and print profile
// replaces the old one.
struct LibrarySave {
    std::optional<std::string> entry; // none: a new entry
    std::filesystem::path model;      // the project (3MF)
    std::filesystem::path settings;   // the full settings the slice used
    std::filesystem::path recipe;     // the app's recipe (printer, nozzle, filament, print, ...), kept as it is
    std::optional<std::filesystem::path> summary, analysis, analysis_summary, thumb;
    std::optional<std::string> name;
    std::string engine; // the engine's version
};
nlohmann::json library_save(const std::filesystem::path& dir, const LibrarySave& request);

// Whether a recipe still matches (rule 7): the same settings, an older engine,
// keys this engine no longer knows, and for each analysis whether the
// settings that change the strength are the same.
nlohmann::json library_check(const std::filesystem::path& dir, const std::string& id, const std::string& recipe,
                             const std::filesystem::path& settings, const std::string& engine);

// «La imprimí» (rule 1): the date, an optional photo kept under photos/ and a note.
nlohmann::json library_printed(const std::filesystem::path& dir, const std::string& id, const std::string& date,
                               const std::optional<std::filesystem::path>& photo, const std::string& note);

// Removes an entry, or one of its recipes or analyses (rule 8).
void library_delete(const std::filesystem::path& dir, const std::string& id, const std::optional<std::string>& recipe,
                    const std::optional<std::string>& analysis);

// An entry as one .tismalib file (rule 10): a ZIP with the entry's folder.
void library_export(const std::filesystem::path& dir, const std::string& id, const std::filesystem::path& file);

// A .tismalib file into the library. Paths that leave the folder, programs
// and files the format does not have refuse it whole, writing nothing. An
// entry that is already there: LibraryError "exists", unless `replace` or
// `as_new`.
enum class LibraryImport { Ask, Replace, AsNew };
nlohmann::json library_import(const std::filesystem::path& file, const std::filesystem::path& dir, LibraryImport mode);

} // namespace tisma
