#include "imgui_editor_internal.hpp"

#include <imgui.h>

#include <algorithm>

namespace gview {
namespace {

const char* factor_name(glayout::FormFactor value) {
    if (value == glayout::FormFactor::Phone) return "Phone";
    if (value == glayout::FormFactor::Tablet) return "Tablet";
    return "Desktop";
}

void apply_preset(AuthoringUiState& state, AuthoringHooks& hooks,
                  const PreviewPreset& preset) {
    state.preview.width = preset.width;
    state.preview.height = preset.height;
    state.preview.output_width = preset.output_width;
    state.preview.output_height = preset.output_height;
    state.preview.device_pixel_ratio = preset.device_pixel_ratio;
    state.preview.form_factor = preset.form_factor;
    state.preview.safe_area = preset.safe_area;
    if (hooks.apply_preview) hooks.apply_preview(state.preview);
    if (hooks.resize_output) hooks.resize_output(preset.output_width, preset.output_height);
}

} // namespace

// Provides independent logical, output, density, scale, safe-area, and camera
// simulation without conflating device pixels with UI sizing.
void draw_display_simulator(AuthoringUiState& state, AuthoringHooks& hooks) {
    if (!state.show_display) return;
    ImGui::SetNextWindowPos({454.0f, 56.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({344.0f, 510.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("GView: Display Simulator", &state.show_display)) {
        ImGui::End();
        return;
    }

    const std::vector<PreviewPreset>& presets = preview_presets();
    const char* current = "Custom";
    for (const PreviewPreset& preset : presets)
        if (preset.width == state.preview.width && preset.height == state.preview.height &&
            preset.output_width == state.preview.output_width &&
            preset.output_height == state.preview.output_height &&
            preset.form_factor == state.preview.form_factor) {
            current = preset.label;
            break;
        }
    if (ImGui::BeginCombo("Device / resolution preset", current)) {
        const char* group = nullptr;
        for (const PreviewPreset& preset : presets) {
            if (!group || std::string_view(group) != preset.group) {
                ImGui::SeparatorText(preset.group);
                group = preset.group;
            }
            const bool selected = current == preset.label;
            if (ImGui::Selectable(preset.label, selected)) apply_preset(state, hooks, preset);
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    bool logical_changed = false;
    logical_changed |= ImGui::InputInt("Logical width", &state.preview.width);
    logical_changed |= ImGui::InputInt("Logical height", &state.preview.height);
    state.preview.width = std::max(1, state.preview.width);
    state.preview.height = std::max(1, state.preview.height);
    if (logical_changed && hooks.apply_preview) hooks.apply_preview(state.preview);

    bool output_changed = false;
    output_changed |= ImGui::InputInt("Output width", &state.preview.output_width);
    output_changed |= ImGui::InputInt("Output height", &state.preview.output_height);
    state.preview.output_width = std::max(160, state.preview.output_width);
    state.preview.output_height = std::max(144, state.preview.output_height);
    if (output_changed && hooks.resize_output)
        hooks.resize_output(state.preview.output_width, state.preview.output_height);

    bool scale_changed = false;
    scale_changed |= ImGui::DragFloat("Device pixel ratio", &state.preview.device_pixel_ratio,
                                      0.05f, 0.5f, 4.0f, "%.2fx");
    scale_changed |= ImGui::DragFloat("UI scale", &state.preview.ui_scale, 0.05f, 0.5f, 3.0f,
                                      "%.2fx");
    state.preview.dpi_scale = state.preview.ui_scale;
    if (scale_changed && hooks.apply_preview) hooks.apply_preview(state.preview);

    int factor = static_cast<int>(state.preview.form_factor);
    constexpr const char* factors[]{"Desktop / TV", "Tablet / handheld", "Phone / portrait"};
    if (ImGui::Combo("Layout form factor", &factor, factors, 3)) {
        state.preview.form_factor = static_cast<glayout::FormFactor>(factor);
        if (hooks.apply_preview) hooks.apply_preview(state.preview);
    }
    ImGui::Text("Active class: %s", factor_name(state.preview.form_factor));

    int presentation = static_cast<int>(state.preview.presentation);
    constexpr const char* presentations[]{"Fit / letterbox", "Stretch", "Overscan",
                                           "Integer scale"};
    if (ImGui::Combo("Output presentation", &presentation, presentations, 4)) {
        state.preview.presentation = static_cast<PreviewPresentation>(presentation);
        if (hooks.apply_preview) hooks.apply_preview(state.preview);
    }
    int sampling = static_cast<int>(state.preview.sampling);
    constexpr const char* samplings[]{"Linear", "Nearest"};
    if (ImGui::Combo("Output sampling", &sampling, samplings, 2)) {
        state.preview.sampling = static_cast<PreviewSampling>(sampling);
        if (hooks.apply_preview) hooks.apply_preview(state.preview);
    }

    float safe[4]{state.preview.safe_area.left, state.preview.safe_area.top,
                  state.preview.safe_area.right, state.preview.safe_area.bottom};
    if (ImGui::DragFloat4("Safe area L/T/R/B", safe, 1.0f, 0.0f, 512.0f)) {
        state.preview.safe_area = {safe[0], safe[1], safe[2], safe[3]};
        if (hooks.apply_preview) hooks.apply_preview(state.preview);
    }
    ImGui::DragFloat("Preview zoom", &state.preview.zoom, 0.02f, 0.25f, 4.0f, "%.2fx");
    ImGui::DragFloat2("Preview pan", &state.preview.pan_x, 1.0f, -4096.0f, 4096.0f);
    if (ImGui::Button("Reset view camera")) {
        state.preview.zoom = 1.0f;
        state.preview.pan_x = 0.0f;
        state.preview.pan_y = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Match output to logical") && hooks.resize_output) {
        state.preview.output_width = state.preview.width;
        state.preview.output_height = state.preview.height;
        hooks.resize_output(state.preview.output_width, state.preview.output_height);
    }
    ImGui::End();
}

} // namespace gview
