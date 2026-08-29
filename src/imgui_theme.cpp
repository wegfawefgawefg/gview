#include "imgui_editor_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace gview {
namespace {

constexpr std::array<const char*, 6> image_modes{"Natural", "Stretch", "Contain",
                                                 "Cover",   "Tile",    "Nine slice"};

bool edit_string(const char* label, std::string& value) {
    std::array<char, 256> buffer{};
    std::strncpy(buffer.data(), value.c_str(), buffer.size() - 1);
    if (!ImGui::InputText(label, buffer.data(), buffer.size())) return false;
    value = buffer.data();
    return true;
}

void live_change(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                 View before, bool edited) {
    if (edited && !state.transaction) state.transaction = std::move(before);
    if (edited && hooks.rebuild) hooks.rebuild();
    if (ImGui::IsItemDeactivatedAfterEdit() && state.transaction) {
        session.commit_snapshot(std::move(*state.transaction));
        state.transaction.reset();
    }
}

void commit_change(AuthoringSession& session, AuthoringHooks& hooks, View before) {
    session.commit_snapshot(std::move(before));
    if (hooks.rebuild) hooks.rebuild();
}

std::vector<const char*> labels(const std::vector<std::string>& storage) {
    std::vector<const char*> result;
    result.reserve(storage.size());
    for (const std::string& value : storage)
        result.push_back(value.c_str());
    return result;
}

std::string skin_label(const WidgetSkin& skin, std::size_t index) {
    std::string result = std::to_string(index + 1) + ": ";
    result += skin.any_control ? "any control" : std::string(to_string(skin.control));
    if (!skin.style_class.empty()) result += " · ." + skin.style_class;
    if (!skin.node_id.empty()) result += " · #" + skin.node_id;
    return result;
}

std::string part_label(const PartPresentation& part, std::size_t index) {
    return std::to_string(index + 1) + ": " + std::string(to_string(part.part)) + " · " +
           std::string(to_string(part.state));
}

void draw_part(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
               PartPresentation& part) {
    View before = session.view();
    const bool asset_edited = edit_string("Asset", part.asset);
    live_change(session, state, hooks, std::move(before), asset_edited);

    int mode = static_cast<int>(part.image_mode);
    if (ImGui::Combo("Image mode", &mode, image_modes.data(), image_modes.size())) {
        before = session.view();
        part.image_mode = static_cast<ImageMode>(mode);
        commit_change(session, hooks, std::move(before));
    }

    float tint[4]{
        static_cast<float>(part.tint.r) / 255.0f, static_cast<float>(part.tint.g) / 255.0f,
        static_cast<float>(part.tint.b) / 255.0f, static_cast<float>(part.tint.a) / 255.0f};
    before = session.view();
    const bool tint_edited = ImGui::ColorEdit4("Tint", tint);
    if (tint_edited) {
        const auto channel = [](float value) {
            return static_cast<std::uint8_t>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        };
        part.tint = {channel(tint[0]), channel(tint[1]), channel(tint[2]), channel(tint[3])};
    }
    live_change(session, state, hooks, std::move(before), tint_edited);

    before = session.view();
    const bool opacity_edited = ImGui::SliderFloat("Opacity", &part.opacity, 0.0f, 1.0f);
    live_change(session, state, hooks, std::move(before), opacity_edited);
    if (part.image_mode != ImageMode::NineSlice) return;

    ImGui::SeparatorText("Nine-slice geometry");
    bool uniform = part.slice_margins.left < 0.0f;
    if (ImGui::Checkbox("Uniform source inset", &uniform)) {
        before = session.view();
        part.slice_margins =
            uniform ? SliceMargins{} : SliceMargins{part.slice, part.slice, part.slice, part.slice};
        commit_change(session, hooks, std::move(before));
    }
    if (uniform) {
        before = session.view();
        const bool edited =
            ImGui::DragFloat("Source inset", &part.slice, 0.25f, 0.0f, 4096.0f, "%.2f px");
        live_change(session, state, hooks, std::move(before), edited);
    } else {
        float margins[4]{part.slice_margins.left, part.slice_margins.top, part.slice_margins.right,
                         part.slice_margins.bottom};
        before = session.view();
        const bool edited =
            ImGui::DragFloat4("Source L/T/R/B", margins, 0.25f, 0.0f, 4096.0f, "%.2f px");
        if (edited) part.slice_margins = {margins[0], margins[1], margins[2], margins[3]};
        live_change(session, state, hooks, std::move(before), edited);
    }
    before = session.view();
    const bool scale_edited =
        ImGui::DragFloat("Rendered border scale", &part.slice_scale, 0.01f, 0.0f, 32.0f, "%.2fx");
    live_change(session, state, hooks, std::move(before), scale_edited);
    ImGui::Checkbox("Show guides on native canvas", &state.show_slice_guides);
    ImGui::TextDisabled("Inset cuts source pixels; scale changes rendered edge thickness.");
}

} // namespace

// Edits the same theme recipes compiled by C++ and S-expression authoring.
void draw_theme_editor(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks) {
    if (!state.show_theme) return;
    ImGui::SetNextWindowPos({470.0f, 56.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({540.0f, 600.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.98f);
    if (!ImGui::Begin("GView: Theme & Assets", &state.show_theme)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Undo") && session.undo() && hooks.rebuild) hooks.rebuild();
    ImGui::SameLine();
    if (ImGui::Button("Redo") && session.redo() && hooks.rebuild) hooks.rebuild();
    ImGui::SameLine();
    if (ImGui::Button("Save") && !session.save()) session.save_as(hooks.default_save_path);
    ImGui::SameLine();
    if (ImGui::Button("Reload") && session.reload() && hooks.rebuild) hooks.rebuild();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", session.dirty() ? "modified" : "saved");
    ImGui::Separator();
    View& view = session.view();
    if (view.themes.empty()) {
        ImGui::TextDisabled("This view has no theme recipes.");
        ImGui::End();
        return;
    }

    std::vector<std::string> theme_names;
    for (const Theme& theme : view.themes)
        theme_names.push_back(theme.id);
    const std::vector<const char*> theme_labels = labels(theme_names);
    int active = 0;
    for (std::size_t index = 0; index < view.themes.size(); ++index)
        if (view.themes[index].id == view.active_theme) active = static_cast<int>(index);
    if (ImGui::Combo("Active theme", &active, theme_labels.data(),
                     static_cast<int>(theme_labels.size()))) {
        const View before = view;
        view.active_theme = view.themes[static_cast<std::size_t>(active)].id;
        commit_change(session, hooks, before);
    }
    state.selected_theme =
        std::clamp(state.selected_theme, 0, static_cast<int>(view.themes.size()) - 1);
    ImGui::Combo("Edit theme", &state.selected_theme, theme_labels.data(),
                 static_cast<int>(theme_labels.size()));
    Theme& theme = view.themes[static_cast<std::size_t>(state.selected_theme)];
    if (theme.widgets.empty()) {
        ImGui::TextDisabled("The selected theme has no widget recipes.");
        ImGui::End();
        return;
    }

    std::vector<std::string> skin_names;
    for (std::size_t index = 0; index < theme.widgets.size(); ++index)
        skin_names.push_back(skin_label(theme.widgets[index], index));
    const std::vector<const char*> skin_labels = labels(skin_names);
    state.selected_widget_skin =
        std::clamp(state.selected_widget_skin, 0, static_cast<int>(theme.widgets.size()) - 1);
    ImGui::Combo("Widget recipe", &state.selected_widget_skin, skin_labels.data(),
                 static_cast<int>(skin_labels.size()));
    WidgetSkin& skin = theme.widgets[static_cast<std::size_t>(state.selected_widget_skin)];
    if (skin.parts.empty()) {
        ImGui::TextDisabled("The selected recipe has no presentation parts.");
        ImGui::End();
        return;
    }

    std::vector<std::string> part_names;
    for (std::size_t index = 0; index < skin.parts.size(); ++index)
        part_names.push_back(part_label(skin.parts[index], index));
    const std::vector<const char*> part_labels = labels(part_names);
    state.selected_widget_part =
        std::clamp(state.selected_widget_part, 0, static_cast<int>(skin.parts.size()) - 1);
    ImGui::Combo("Presentation part", &state.selected_widget_part, part_labels.data(),
                 static_cast<int>(part_labels.size()));
    PartPresentation& part = skin.parts[static_cast<std::size_t>(state.selected_widget_part)];
    state.slice_guide_asset = part.image_mode == ImageMode::NineSlice ? part.asset : "";
    draw_part(session, state, hooks, part);
    ImGui::End();
}

} // namespace gview
