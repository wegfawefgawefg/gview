#pragma once

#include "gview/widget.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace gview {

// Names presentation states independently from retained interaction storage.
enum class PresentationState {
    Normal,
    Selected,
    Hovered,
    Focused,
    SelectedFocused,
    Pressed,
    Disabled,
    Open,
    On,
    Off,
    Count
};

// Defines how an asset maps into one resolved widget part.
enum class ImageMode { Natural, Stretch, Contain, Cover, Tile, NineSlice };

// Overrides one state of one semantic widget part.
struct PartPresentation {
    WidgetPart part = WidgetPart::Frame;
    PresentationState state = PresentationState::Normal;
    BoxStyle box;
    bool override_box = false;
    std::string asset;
    ImageMode image_mode = ImageMode::Stretch;
    Color tint{255, 255, 255, 255};
    float opacity = 1.0f;
    float slice = 8.0f;
};

// Selects reusable, class-specific, or node-specific compound control styling.
struct WidgetSkin {
    ControlKind control = ControlKind::None;
    bool any_control = false;
    std::string style_class;
    std::string node_id;
    std::vector<PartPresentation> parts;
};

// Holds one inheritance layer of game-provided presentation recipes.
struct Theme {
    std::string id;
    std::string extends;
    std::vector<WidgetSkin> widgets;
};

constexpr std::size_t widget_part_count = static_cast<std::size_t>(WidgetPart::Count);
constexpr std::size_t presentation_state_count =
    static_cast<std::size_t>(PresentationState::Count);

// Stores dense resolved part styles compiled once for one semantic node.
struct CompiledSkin {
    static constexpr std::uint16_t missing = std::numeric_limits<std::uint16_t>::max();
    std::array<std::uint16_t, widget_part_count * presentation_state_count> slots;
    std::vector<PartPresentation> parts;

    CompiledSkin() { slots.fill(missing); }
};

CompiledSkin compile_skin(const std::vector<Theme>& themes, std::string_view active_theme,
                          const NodeSpec& node);
const PartPresentation* find_part(const CompiledSkin& skin, WidgetPart part,
                                  PresentationState state);
PresentationState presentation_state(const NodeSpec& node, bool hovered, bool pressed,
                                     bool open, bool focused, const Value& bound);

std::string_view to_string(WidgetPart value);
std::string_view to_string(PresentationState value);
std::string_view to_string(ImageMode value);

} // namespace gview
