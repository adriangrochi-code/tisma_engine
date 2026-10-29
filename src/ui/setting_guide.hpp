#pragma once

#include <cstddef>
#include <string_view>

// A longer explanation of a setting for someone who never used a slicer:
// what it is, what it changes in the part and an example (the owner's
// request, 2026-10-29). Own texts; see PROVENANCE.md.
namespace tisma::ui {

// The explanation in the language in use, or empty when the setting has
// none yet (its card then shows the short meaning from docs/CONFIG.md).
std::string_view setting_guide(std::string_view key);

// The keys that have one, for the tests.
std::size_t setting_guide_count();
std::string_view setting_guide_key(std::size_t i);

} // namespace tisma::ui
