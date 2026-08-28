#include "gview/theme.hpp"

#include <algorithm>
#include <unordered_set>

namespace gview {
namespace {

std::size_t index(WidgetPart part, PresentationState state) {
    return static_cast<std::size_t>(part) * presentation_state_count +
           static_cast<std::size_t>(state);
}

bool matches(const WidgetSkin& skin, const NodeSpec& node) {
    if (!skin.any_control && skin.control != node.control) return false;
    if (!skin.style_class.empty() && skin.style_class != node.style_class) return false;
    return skin.node_id.empty() || skin.node_id == node.layout_id;
}

const Theme* find_theme(const std::vector<Theme>& themes, std::string_view id) {
    const auto found = std::find_if(themes.begin(), themes.end(), [&](const Theme& theme) {
        return theme.id == id;
    });
    return found == themes.end() ? nullptr : &*found;
}

void apply_theme(const std::vector<Theme>& themes, const Theme& theme, const NodeSpec& node,
                 std::unordered_set<std::string>& visiting, CompiledSkin& output) {
    if (!visiting.insert(theme.id).second) return;
    if (!theme.extends.empty()) {
        const Theme* parent = find_theme(themes, theme.extends);
        if (parent) apply_theme(themes, *parent, node, visiting, output);
    }
    for (const WidgetSkin& widget : theme.widgets) {
        if (!matches(widget, node)) continue;
        for (const PartPresentation& part : widget.parts) {
            const std::size_t slot = index(part.part, part.state);
            const std::uint16_t existing = output.slots[slot];
            if (existing == CompiledSkin::missing) {
                output.slots[slot] = static_cast<std::uint16_t>(output.parts.size());
                output.parts.push_back(part);
            } else
                output.parts[existing] = part;
        }
    }
    visiting.erase(theme.id);
}

} // namespace

// Resolves inheritance and selector precedence before the runtime hot path.
CompiledSkin compile_skin(const std::vector<Theme>& themes, std::string_view active_theme,
                          const NodeSpec& node) {
    CompiledSkin result;
    const Theme* theme = find_theme(themes, active_theme);
    if (!theme) return result;
    std::unordered_set<std::string> visiting;
    apply_theme(themes, *theme, node, visiting, result);
    return result;
}

const PartPresentation* find_part(const CompiledSkin& skin, WidgetPart part,
                                  PresentationState state) {
    const std::size_t exact = index(part, state);
    if (skin.slots[exact] != CompiledSkin::missing) return &skin.parts[skin.slots[exact]];
    const std::size_t normal = index(part, PresentationState::Normal);
    return skin.slots[normal] != CompiledSkin::missing ? &skin.parts[skin.slots[normal]] : nullptr;
}

// Maps retained interaction and bound values to one presentation state.
PresentationState presentation_state(const NodeSpec& node, bool hovered, bool pressed,
                                     bool open, bool focused, const Value& bound) {
    if (!node.enabled) return PresentationState::Disabled;
    if (pressed) return PresentationState::Pressed;
    if (open) return PresentationState::Open;
    if (node.selected && focused) return PresentationState::SelectedFocused;
    if (focused) return PresentationState::Focused;
    if (hovered) return PresentationState::Hovered;
    if (node.selected) return PresentationState::Selected;
    if (const bool* value = std::get_if<bool>(&bound))
        return *value ? PresentationState::On : PresentationState::Off;
    return PresentationState::Normal;
}

std::string_view to_string(WidgetPart value) {
    switch (value) {
    case WidgetPart::Frame: return "frame";
    case WidgetPart::Label: return "label";
    case WidgetPart::Description: return "description";
    case WidgetPart::CurrentValue: return "value";
    case WidgetPart::Track: return "track";
    case WidgetPart::Fill: return "fill";
    case WidgetPart::Thumb: return "thumb";
    case WidgetPart::Indicator: return "indicator";
    case WidgetPart::Popup: return "popup";
    case WidgetPart::Option: return "option";
    case WidgetPart::Count: return "frame";
    }
    return "frame";
}

std::string_view to_string(PresentationState value) {
    switch (value) {
    case PresentationState::Normal: return "normal";
    case PresentationState::Selected: return "selected";
    case PresentationState::Hovered: return "hovered";
    case PresentationState::Focused: return "focused";
    case PresentationState::SelectedFocused: return "selected_focused";
    case PresentationState::Pressed: return "pressed";
    case PresentationState::Disabled: return "disabled";
    case PresentationState::Open: return "open";
    case PresentationState::On: return "on";
    case PresentationState::Off: return "off";
    case PresentationState::Count: return "normal";
    }
    return "normal";
}

std::string_view to_string(ImageMode value) {
    switch (value) {
    case ImageMode::Natural: return "natural";
    case ImageMode::Stretch: return "stretch";
    case ImageMode::Contain: return "contain";
    case ImageMode::Cover: return "cover";
    case ImageMode::Tile: return "tile";
    case ImageMode::NineSlice: return "nine_slice";
    }
    return "stretch";
}

} // namespace gview
