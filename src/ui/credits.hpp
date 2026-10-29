#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Where the interface's functions come from (docs/spec/features/credits.md):
// the «Based on» line under a card and the Credits and sources page, both
// read from one data file, resources/credits.json. Own design on that spec.
namespace tisma::ui {

// A text in the interface's two languages; the names of sources and authors
// are not translated (rule 7), the rest is.
struct Localized {
    std::string es, en;
    // The one for the interface's language now.
    const std::string& now() const;
};

// A function taken from another source (rule 2): what it is, the spec that
// says so ("support_fins.md"; empty when only PROVENANCE.md does), where it
// comes from and the settings cards it shows under (the panel's subsection
// ids, settings_layout.hpp).
struct FeatureCredit {
    Localized feature;
    std::string spec;
    std::string source;
    std::string short_source; // the name under a card, when `source` is a paper's long title
    std::string author;
    Localized license;
    std::string url;
    std::vector<std::string> cards;
};

// A library the program carries (THIRD_PARTY.md), with its license notices:
// the names of files in resources/licenses/.
struct LibraryCredit {
    std::string name, version, license, url;
    std::vector<std::string> notices;
};

struct Credits {
    std::vector<FeatureCredit> features;
    std::vector<LibraryCredit> libraries;
};

class CreditsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
// The data file ({"format": "tisma-credits", "version": 1, ...}); throws
// CreditsError when the text is not one.
Credits parse_credits(std::string_view json);

// The credits a settings card shows, in the file's order.
std::vector<const FeatureCredit*> credits_for_card(const Credits& credits, std::string_view card);

// The source's name under a card: its short one, if it has one.
const std::string& card_source(const FeatureCredit& credit);

// «Basado en Support Fins, de gittrahan» in the interface's language (rule 3),
// with the name under a card.
std::string based_on(const FeatureCredit& credit);

// A row of THIRD_PARTY.md's table of libraries: the name, the version as
// written there, the license and the URL; `distributed` is false for the
// ones only the tests use («Solo pruebas»).
struct ThirdPartyRow {
    std::string name, version, license, url;
    bool distributed = true;
};
std::vector<ThirdPartyRow> third_party_rows(std::string_view markdown);

// The URLs a spec's «**Basado en:**» line names (`https://...` in back
// quotes); empty when the spec has no such line.
std::vector<std::string> based_on_urls(std::string_view spec_markdown);

} // namespace tisma::ui
