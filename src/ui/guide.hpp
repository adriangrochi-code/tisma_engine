#pragma once

#include "ui/knowledge.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// The help's guided tutorials (docs/spec/features/assistant.md, rules 15 to
// 18): which ones to offer and when, how a step moves on, and where a
// tutorial left off. Own design on that spec.
namespace tisma::ui {

// What the interface remembers of the tutorials (rule 18): the step each one
// reached, and those turned down or finished, which are not offered again.
struct GuideMemory {
    std::map<std::string, int> step;
    std::set<std::string> dismissed, done;
};

// The tutorials to offer at a moment (rule 15): "first_start",
// "new_filament" or "first_open:<part>"; on_request ones never offer
// themselves, nor those turned down or done.
std::vector<std::size_t> tutorials_to_offer(const Knowledge& knowledge, std::string_view when,
                                            const GuideMemory& memory);

// A tutorial going on: which one and its step.
struct Guide {
    std::size_t tutorial = 0;
    int step = 0;
};

// Starts a tutorial where it was left (rule 18), or at its beginning.
Guide start_guide(const Knowledge& knowledge, std::size_t tutorial, const GuideMemory& memory);

// Something happened in the interface ("model_added", "sliced"...): the step
// that waits for it moves on (rule 16). True when that ends the tutorial.
bool guide_event(const Knowledge& knowledge, Guide& guide, std::string_view event);

// Next and Back by hand; Next on the last step ends it (true).
bool guide_next(const Knowledge& knowledge, Guide& guide);
void guide_back(Guide& guide);

// What the memory keeps of a guide left now: its step, or done.
void remember(GuideMemory& memory, const Knowledge& knowledge, const Guide& guide, bool finished);

} // namespace tisma::ui
