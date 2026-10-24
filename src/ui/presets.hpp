#pragma once

#include "config/profiles.hpp"

#include <vector>

// The profiles the interface comes with (docs/PROFILES.md): generic ones, so
// that there is something to start from. Own choice of values, from common
// practice; see PROVENANCE.md.
namespace tisma::ui {

// For each kind, the first one is what default_settings() has: a generic
// Marlin printer, PLA, and 0.20 mm layers. Their names are the same in every
// language, and users cannot save over them.
std::vector<Profile> builtin_profiles(ProfileKind kind);

} // namespace tisma::ui
