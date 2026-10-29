#pragma once

#include <cstddef>
#include <string_view>

namespace tisma::ui {

// Files compiled into the program (cmake/embed.cmake): the Tisma icons of
// assets/brand/, the interface font and docs/CONFIG.md (the settings' help).
struct EmbeddedFile {
    const char* name; // the file's name, without folders
    const unsigned char* data;
    std::size_t size; // bytes (data has one more, a 0, so text can be read as C text)
};
extern const EmbeddedFile kEmbeddedFiles[];
extern const std::size_t kEmbeddedFileCount;

// The file of that name, or an empty view.
inline std::string_view embedded(std::string_view name) {
    for (std::size_t i = 0; i < kEmbeddedFileCount; ++i)
        if (name == kEmbeddedFiles[i].name)
            return {reinterpret_cast<const char*>(kEmbeddedFiles[i].data), kEmbeddedFiles[i].size};
    return {};
}

} // namespace tisma::ui
