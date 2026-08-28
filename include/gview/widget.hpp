#pragma once

#include "gview/types.hpp"

#include <glayout/layout.hpp>

#include <cstddef>
#include <vector>

namespace gview {

// Names the semantic slots inside a compound control presentation.
enum class WidgetPart {
    Frame,
    Label,
    Description,
    CurrentValue,
    Track,
    Fill,
    Thumb,
    Indicator,
    Popup,
    Option,
    Count
};

// Holds resolved control slots without turning decorative parts into focus nodes.
struct WidgetGeometry {
    glayout::Rect frame;
    glayout::Rect label;
    glayout::Rect description;
    glayout::Rect value;
    glayout::Rect track;
    glayout::Rect fill;
    glayout::Rect thumb;
    glayout::Rect indicator;
};

// Describes one viewport-aware select popup and its visible option rows.
struct PopupGeometry {
    glayout::Rect frame;
    std::vector<glayout::Rect> options;
    std::size_t first_option = 0;
};

WidgetGeometry resolve_widget_geometry(const NodeSpec& node, glayout::Rect content,
                                       double value_ratio = 0.0);
PopupGeometry resolve_popup_geometry(const NodeSpec& node, glayout::Rect anchor,
                                     glayout::Rect viewport, std::size_t selected_option);

} // namespace gview
