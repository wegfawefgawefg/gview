#include "gview/paint.hpp"
#include "gview/runtime.hpp"

namespace gview {

// Chooses visual state without conflating persistent selection and transient focus.
const BoxStyle& resolve_style(const NodeSpec& node, const NodeState& state, bool focused) {
    if (!node.enabled)
        return node.style.disabled;
    if (state.pressed)
        return node.style.pressed;
    if (focused && node.selected)
        return node.style.selected_focused;
    if (focused)
        return node.style.focused;
    if (state.hovered)
        return node.style.hovered;
    if (node.selected)
        return node.style.selected;
    return node.style.normal;
}

// Normalizes numeric binding variants for sliders and progress presentation.
double numeric_value(const Value& value, double fallback) {
    if (const double* number = std::get_if<double>(&value))
        return *number;
    if (const std::int64_t* integer = std::get_if<std::int64_t>(&value))
        return static_cast<double>(*integer);
    return fallback;
}

} // namespace gview
