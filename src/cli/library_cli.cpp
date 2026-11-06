// My library on the command line. Own code; see library_cli.hpp.

#include "cli/library_cli.hpp"

#include "cli/cli.hpp"
#include "io/zip.hpp"
#include "library/library.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace tisma {
namespace {

constexpr std::string_view kLibraryUsage =
    "usage: tisma-engine library <command> --dir <library folder> ...\n"
    "  list\n"
    "  show --entry ID\n"
    "  save (--new | --entry ID) --model P.3mf --config S.json --recipe R.json [--summary S.json]\n"
    "       [--analysis A.tea|A.taero [--analysis-summary A.json]] [--name N] [--thumb T.png]\n"
    "  check --entry ID --recipe N --config S.json\n"
    "  printed --entry ID [--date ISO] [--photo P.jpg] [--note TEXT]\n"
    "  delete --entry ID [--recipe N | --analysis N]\n"
    "  export --entry ID -o F.tismalib\n"
    "  import F.tismalib [--replace | --new]\n";

std::optional<std::string> option(std::span<const std::string_view> args, std::string_view name) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i)
        if (args[i] == name)
            return std::string(args[i + 1]);
    return std::nullopt;
}

bool flag(std::span<const std::string_view> args, std::string_view name) {
    return std::find(args.begin(), args.end(), name) != args.end();
}

} // namespace

int cmd_library(std::span<const std::string_view> args, std::ostream& out, std::ostream& err) {
    if (args.empty()) {
        out << kLibraryUsage;
        return kExitOk;
    }
    const std::string_view what = args[0];
    const auto rest = args.subspan(1);
    const auto need = [&](std::string_view name) {
        if (const auto v = option(rest, name))
            return *v;
        throw LibraryError("usage", "missing " + std::string(name), "falta " + std::string(name));
    };
    const auto path = [&](std::string_view name) -> std::optional<std::filesystem::path> {
        if (const auto v = option(rest, name))
            return std::filesystem::path(*v);
        return std::nullopt;
    };
    try {
        if (what == "import") {
            if (rest.empty() || rest[0].starts_with("--"))
                throw LibraryError("usage", "missing the .tismalib file", "falta el archivo .tismalib");
            const LibraryImport mode = flag(rest, "--replace") ? LibraryImport::Replace
                                       : flag(rest, "--new")   ? LibraryImport::AsNew
                                                               : LibraryImport::Ask;
            out << library_import(std::filesystem::path(std::string(rest[0])), need("--dir"), mode).dump() << "\n";
            return kExitOk;
        }
        const std::filesystem::path dir = need("--dir");
        if (what == "list") {
            out << library_list(dir).dump() << "\n";
        } else if (what == "show") {
            out << library_show(dir, need("--entry")).dump() << "\n";
        } else if (what == "save") {
            LibrarySave r;
            if (!flag(rest, "--new"))
                r.entry = need("--entry");
            r.model = need("--model");
            r.settings = need("--config");
            r.recipe = need("--recipe");
            r.summary = path("--summary");
            r.analysis = path("--analysis");
            r.analysis_summary = path("--analysis-summary");
            r.thumb = path("--thumb");
            r.name = option(rest, "--name");
            r.engine = TISMA_VERSION;
            out << library_save(dir, r).dump() << "\n";
        } else if (what == "check") {
            out << library_check(dir, need("--entry"), need("--recipe"), need("--config"), TISMA_VERSION).dump() << "\n";
        } else if (what == "printed") {
            out << library_printed(dir, need("--entry"), option(rest, "--date").value_or(""), path("--photo"),
                                   option(rest, "--note").value_or(""))
                       .dump()
                << "\n";
        } else if (what == "delete") {
            library_delete(dir, need("--entry"), option(rest, "--recipe"), option(rest, "--analysis"));
            out << "{}\n";
        } else if (what == "export") {
            library_export(dir, need("--entry"), need("-o"));
            out << "{}\n";
        } else {
            err << "error: unknown library command '" << what << "'\n" << kLibraryUsage;
            return kExitUsage;
        }
        return kExitOk;
    } catch (const LibraryError& e) {
        if (e.code() == "exists") {
            out << nlohmann::json{{"exists", e.what()}}.dump() << "\n";
            return kExitNotImplemented; // 3: the caller asks «Reemplazar» or «Guardar como nueva»
        }
        out << nlohmann::json{{"code", e.code()}, {"error", e.what()}, {"error_es", e.message_es()}}.dump() << "\n";
        return kExitInput;
    } catch (const ZipError& e) {
        out << nlohmann::json{{"code", "input"}, {"error", std::string("damaged file: ") + e.what()},
                              {"error_es", "el archivo está dañado"}}
                   .dump()
            << "\n";
        return kExitInput;
    } catch (const std::filesystem::filesystem_error& e) {
        out << nlohmann::json{{"code", "write"}, {"error", e.what()}, {"error_es", "no se pudo escribir en la biblioteca"}}
                   .dump()
            << "\n";
        return kExitInput;
    }
}

} // namespace tisma
