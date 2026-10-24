#pragma once

#include <cstddef>
#include <string_view>

// The catalog's data files compiled into the engine (cmake/embed.cmake, from
// resources/catalog/).
namespace tisma::catalog_data {

struct EmbeddedFile {
    const char* name; // the file's name, without folders
    const unsigned char* data;
    std::size_t size; // bytes (data has one more, a 0)
};
extern const EmbeddedFile kEmbeddedFiles[];
extern const std::size_t kEmbeddedFileCount;

} // namespace tisma::catalog_data

namespace tisma {

// The catalog file of that name, or an empty view.
inline std::string_view catalog_file(std::string_view name) {
    for (std::size_t i = 0; i < catalog_data::kEmbeddedFileCount; ++i)
        if (name == catalog_data::kEmbeddedFiles[i].name)
            return {reinterpret_cast<const char*>(catalog_data::kEmbeddedFiles[i].data),
                    catalog_data::kEmbeddedFiles[i].size};
    return {};
}

} // namespace tisma
