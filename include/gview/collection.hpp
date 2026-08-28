#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace gview {

struct VirtualRange {
    std::size_t first = 0;
    std::size_t last = 0;
    float leading_extent = 0.0f;
    float trailing_extent = 0.0f;
};

VirtualRange virtual_range(std::size_t count, float item_extent, float viewport_extent,
                           float scroll_offset, std::size_t overscan = 2);
std::string collection_item_id(std::string_view collection_id, std::string_view stable_key);

} // namespace gview
