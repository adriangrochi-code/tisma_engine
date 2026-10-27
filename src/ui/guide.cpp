#include "ui/guide.hpp"

#include <algorithm>

namespace tisma::ui {

std::vector<std::size_t> tutorials_to_offer(const Knowledge& k, std::string_view when, const GuideMemory& memory) {
    std::vector<std::size_t> out;
    if (when == "on_request")
        return out;
    for (std::size_t i = 0; i < k.tutorials.size(); ++i) {
        const KnowledgeTutorial& t = k.tutorials[i];
        if (t.when == when && !memory.dismissed.contains(t.id) && !memory.done.contains(t.id))
            out.push_back(i);
    }
    return out;
}

Guide start_guide(const Knowledge& k, std::size_t tutorial, const GuideMemory& memory) {
    Guide g{tutorial, 0};
    if (tutorial < k.tutorials.size())
        if (const auto it = memory.step.find(k.tutorials[tutorial].id); it != memory.step.end())
            g.step = std::clamp(it->second, 0, std::max(0, int(k.tutorials[tutorial].steps.size()) - 1));
    return g;
}

bool guide_next(const Knowledge& k, Guide& g) {
    if (g.tutorial >= k.tutorials.size())
        return true;
    if (g.step + 1 >= int(k.tutorials[g.tutorial].steps.size()))
        return true;
    ++g.step;
    return false;
}

void guide_back(Guide& g) { g.step = std::max(0, g.step - 1); }

bool guide_event(const Knowledge& k, Guide& g, std::string_view event) {
    if (g.tutorial >= k.tutorials.size() || g.step >= int(k.tutorials[g.tutorial].steps.size()))
        return false;
    if (k.tutorials[g.tutorial].steps[std::size_t(g.step)].wait_for != event || event.empty())
        return false;
    return guide_next(k, g);
}

void remember(GuideMemory& memory, const Knowledge& k, const Guide& g, bool finished) {
    if (g.tutorial >= k.tutorials.size())
        return;
    const std::string& id = k.tutorials[g.tutorial].id;
    if (finished) {
        memory.done.insert(id);
        memory.step.erase(id);
    } else {
        memory.step[id] = g.step;
    }
}

} // namespace tisma::ui
