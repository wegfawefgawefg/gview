#include "gview/collection.hpp"

#include <algorithm>
#include <cmath>

namespace gview {

// Computes the materialized interval and spacer extents for a uniform virtual
// collection.
VirtualRange virtual_range(std::size_t count, float item_extent, float viewport_extent,
                           float scroll_offset, std::size_t overscan) {
    if (count == 0 || item_extent <= 0.0f || viewport_extent <= 0.0f) return {};
    const float total = item_extent * static_cast<float>(count);
    const float maximum_scroll = std::max(0.0f, total - viewport_extent);
    const float scroll = std::clamp(scroll_offset, 0.0f, maximum_scroll);
    const std::size_t visible_first = static_cast<std::size_t>(std::floor(scroll / item_extent));
    const std::size_t visible_count =
        static_cast<std::size_t>(std::ceil(viewport_extent / item_extent)) + 1;
    const std::size_t first = visible_first > overscan ? visible_first - overscan : 0;
    const std::size_t last = std::min(count, visible_first + visible_count + overscan);
    return {first, last, static_cast<float>(first) * item_extent,
            static_cast<float>(count - last) * item_extent};
}

// Produces a stable authored identity without coupling collection data to
// layout storage.
std::string collection_item_id(std::string_view collection_id, std::string_view stable_key) {
    std::string result;
    result.reserve(collection_id.size() + stable_key.size() + 1);
    result.append(collection_id);
    result.push_back('/');
    for (const char character : stable_key) {
        const bool safe =
            (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_';
        result.push_back(safe ? character : '-');
    }
    return result;
}

} // namespace gview
