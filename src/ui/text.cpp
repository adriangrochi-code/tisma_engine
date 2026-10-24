// The interface's texts. Own design; see PROVENANCE.md.

#include "ui/text.hpp"

#include <atomic>

namespace tisma::ui {
namespace {

struct Entry {
    const char* es;
    const char* en;
};

constexpr Entry kText[] = {
#define TISMA_UI_TEXT_ENTRY(id, es, en) {es, en},
    TISMA_UI_TEXT(TISMA_UI_TEXT_ENTRY)
#undef TISMA_UI_TEXT_ENTRY
};
static_assert(sizeof(kText) / sizeof(kText[0]) == static_cast<std::size_t>(T::Count));

std::atomic<Language> current{Language::Spanish};

} // namespace

void set_language(Language l) { current = l; }
Language language() { return current; }

const char* tr(T id) {
    const Entry& e = kText[static_cast<int>(id)];
    return current == Language::Spanish ? e.es : e.en;
}

} // namespace tisma::ui
