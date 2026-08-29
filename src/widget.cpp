#include "gview/widget.hpp"

#include <algorithm>
#include <cmath>

namespace gview {
namespace {

glayout::Rect inset(glayout::Rect rect, float horizontal, float vertical) {
    const float x = std::min(horizontal, rect.w * 0.5f);
    const float y = std::min(vertical, rect.h * 0.5f);
    return {rect.x + x, rect.y + y, std::max(0.0f, rect.w - x * 2.0f),
            std::max(0.0f, rect.h - y * 2.0f)};
}

float clamped_ratio(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

} // namespace

// Resolves stable semantic slots for controls independently from their skin.
WidgetGeometry resolve_widget_geometry(const NodeSpec& node, glayout::Rect content,
                                       double value_ratio) {
    WidgetGeometry result;
    result.frame = content;
    result.label = content;
    result.value = content;

    const float text_height = std::max(12.0f, node.text_style.size * 1.35f);
    const float gap = std::max(4.0f, node.text_style.size * 0.45f);
    if (node.control == ControlKind::Slider) {
        const float track_height = std::clamp(node.text_style.size * 0.55f, 6.0f, 10.0f);
        const float header_height = std::min(text_height, content.h * 0.48f);
        const float thumb_size = std::min(
            std::clamp(node.text_style.size * 1.05f, 14.0f, 24.0f),
            std::max(track_height, content.h - header_height - gap));
        const float used_height = header_height + gap + thumb_size;
        const float top = content.y + std::max(0.0f, (content.h - used_height) * 0.5f);
        const float track_center = top + header_height + gap + thumb_size * 0.5f;
        const float value_width = std::min(content.w * 0.32f, node.text_style.size * 8.0f);
        result.label = {content.x, top, std::max(0.0f, content.w - value_width - gap),
                        header_height};
        result.value = {content.x + content.w - value_width, top, value_width,
                        header_height};
        result.track = {content.x, track_center - track_height * 0.5f, content.w, track_height};
        result.fill = result.track;
        result.fill.w *= clamped_ratio(value_ratio);
        const float thumb_center = result.track.x + result.track.w * clamped_ratio(value_ratio);
        result.thumb = {thumb_center - thumb_size * 0.5f,
                        result.track.y + result.track.h * 0.5f - thumb_size * 0.5f,
                        thumb_size, thumb_size};
        return result;
    }

    if (node.control == ControlKind::Select) {
        const float indicator_width = std::max(24.0f, text_height);
        const float value_width = std::clamp(content.w * 0.48f, 90.0f,
                                             std::max(90.0f, content.w - indicator_width));
        result.label = {content.x, content.y,
                        std::max(0.0f, content.w - value_width - indicator_width - gap),
                        content.h};
        result.value = {content.x + content.w - value_width - indicator_width, content.y,
                        value_width, content.h};
        result.indicator = {content.x + content.w - indicator_width, content.y,
                            indicator_width, content.h};
        return result;
    }

    if (node.control == ControlKind::Toggle) {
        const float indicator_width = std::clamp(content.h * 0.72f, 30.0f, 54.0f);
        result.label = {content.x, content.y,
                        std::max(0.0f, content.w - indicator_width - gap), content.h};
        result.indicator = {content.x + content.w - indicator_width,
                            content.y + (content.h - indicator_width * 0.58f) * 0.5f,
                            indicator_width, indicator_width * 0.58f};
        return result;
    }

    if (node.control == ControlKind::TextInput) {
        result.label = inset(content, gap, 0.0f);
        return result;
    }
    return result;
}

// Places a select popup inside the root viewport and around the selected row.
PopupGeometry resolve_popup_geometry(const NodeSpec& node, glayout::Rect anchor,
                                     glayout::Rect viewport, std::size_t selected_option) {
    PopupGeometry result;
    if (node.options.empty() || viewport.w <= 0.0f || viewport.h <= 0.0f) return result;

    constexpr std::size_t maximum_rows = 8;
    const std::size_t visible = std::min(maximum_rows, node.options.size());
    if (node.options.size() > visible) {
        const std::size_t half = visible / 2;
        result.first_option = selected_option > half ? selected_option - half : 0;
        result.first_option = std::min(result.first_option, node.options.size() - visible);
    }

    const float row_height = std::max(36.0f, node.text_style.size * 2.35f);
    const float popup_height = row_height * static_cast<float>(visible);
    const float popup_width = std::clamp(anchor.w, 180.0f, viewport.w);
    float x = std::clamp(anchor.x, viewport.x, viewport.x + viewport.w - popup_width);
    float y = anchor.y + anchor.h;
    if (y + popup_height > viewport.y + viewport.h)
        y = anchor.y - popup_height;
    y = std::clamp(y, viewport.y, viewport.y + viewport.h - popup_height);
    result.frame = {x, y, popup_width, popup_height};
    result.options.reserve(visible);
    for (std::size_t row = 0; row < visible; ++row)
        result.options.push_back({x, y + row_height * static_cast<float>(row), popup_width,
                                  row_height});
    return result;
}

} // namespace gview
