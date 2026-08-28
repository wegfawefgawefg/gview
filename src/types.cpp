#include "gview/types.hpp"
#include "gview/view.hpp"

#include <algorithm>

namespace gview {

// Exposes stable source spellings for diagnostics and persistence.
std::string_view to_string(ContentKind value) {
    switch (value) {
    case ContentKind::None: return "none";
    case ContentKind::Text: return "text";
    case ContentKind::Image: return "image";
    case ContentKind::Sprite: return "sprite";
    case ContentKind::Progress: return "progress";
    case ContentKind::CustomSurface: return "custom_surface";
    }
    return "none";
}

std::string_view to_string(ControlKind value) {
    switch (value) {
    case ControlKind::None: return "none";
    case ControlKind::Button: return "button";
    case ControlKind::Toggle: return "toggle";
    case ControlKind::Slider: return "slider";
    case ControlKind::Select: return "select";
    case ControlKind::TextInput: return "text_input";
    case ControlKind::ScrollArea: return "scroll_area";
    }
    return "none";
}

std::string_view to_string(NavAction value) {
    switch (value) {
    case NavAction::Up: return "up";
    case NavAction::Down: return "down";
    case NavAction::Left: return "left";
    case NavAction::Right: return "right";
    case NavAction::Confirm: return "confirm";
    case NavAction::Back: return "back";
    case NavAction::TabPrevious: return "tab_previous";
    case NavAction::TabNext: return "tab_next";
    }
    return "down";
}

// Maintains replace-by-identity behavior for live-loaded view stores.
const View* ViewStore::find(std::string_view id) const {
    const auto found = std::find_if(views.begin(), views.end(),
                                    [&](const View& view) { return view.id == id; });
    return found == views.end() ? nullptr : &*found;
}

void ViewStore::add_or_replace(View view) {
    const auto found = std::find_if(views.begin(), views.end(),
                                    [&](const View& item) { return item.id == view.id; });
    if (found == views.end())
        views.push_back(std::move(view));
    else
        *found = std::move(view);
}

} // namespace gview
