#pragma once

#include "gview/compile.hpp"

#include <glayout/layout.hpp>

namespace gview {

struct NodeState;

enum class PaintKind { Box, Text, Image, Sprite, Progress, CustomSurface };

struct PaintCommand {
    PaintKind kind = PaintKind::Box;
    PaintStratum stratum = PaintStratum::Content;
    NodeIndex node = invalid_node;
    glayout::Rect rect;
    glayout::Rect clip;
    BoxStyle box;
    TextStyle text_style;
    std::string text;
    std::string asset;
    double value = 0.0;
    ImageMode image_mode = ImageMode::Stretch;
    Color tint{255, 255, 255, 255};
    float image_opacity = 1.0f;
    float slice = 8.0f;
    SliceMargins slice_margins;
    float slice_scale = 1.0f;
};

const BoxStyle& resolve_style(const NodeSpec& node, const NodeState& state, bool focused);
double numeric_value(const Value& value, double fallback = 0.0);

} // namespace gview
