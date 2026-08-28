#pragma once

#include "gview/io.hpp"

#include <filesystem>
#include <functional>
#include <optional>

namespace gview {

enum class PreviewPresentation { Fit, Stretch, Overscan, IntegerScale };
enum class PreviewSampling { Linear, Nearest };

struct PreviewConfig {
    int width = 1280;
    int height = 720;
    int output_width = 1280;
    int output_height = 720;
    float dpi_scale = 1.0f;
    float device_pixel_ratio = 1.0f;
    float ui_scale = 1.0f;
    float zoom = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    PreviewPresentation presentation = PreviewPresentation::Fit;
    PreviewSampling sampling = PreviewSampling::Linear;
    glayout::FormFactor form_factor = glayout::FormFactor::Desktop;
    glayout::Insets safe_area;
    std::string state = "default";
};

struct PreviewPreset {
    const char* group = "";
    const char* label = "";
    int width = 1280;
    int height = 720;
    int output_width = 1280;
    int output_height = 720;
    float device_pixel_ratio = 1.0f;
    glayout::FormFactor form_factor = glayout::FormFactor::Desktop;
    glayout::Insets safe_area;

    PreviewPreset(const char* group_value, const char* label_value, int width_value,
                  int height_value, float density_value, glayout::FormFactor factor_value,
                  glayout::Insets safe_value = {})
        : group(group_value), label(label_value),
          width(static_cast<int>(static_cast<float>(width_value) / density_value + 0.5f)),
          height(static_cast<int>(static_cast<float>(height_value) / density_value + 0.5f)),
          output_width(width_value), output_height(height_value),
          device_pixel_ratio(density_value), form_factor(factor_value), safe_area(safe_value) {}
};

const std::vector<PreviewPreset>& preview_presets();

struct AuthoringFragment {
    glayout::GraphNode layout;
    std::vector<NodeSpec> nodes;
    std::vector<FocusEdge> focus_edges;
};

class AuthoringSession {
  public:
    void open(View view, std::filesystem::path source = {});

    const View& view() const;
    View& view();
    const std::filesystem::path& source() const;
    const std::string& selection() const;
    void select(std::string id);
    bool dirty() const;
    bool can_undo() const;
    bool can_redo() const;

    bool edit(const std::function<bool(View&)>& mutation);
    void commit_snapshot(View before);
    bool add(std::string_view parent, glayout::GraphNode layout, NodeSpec presentation);
    bool remove(std::string_view id);
    bool duplicate(std::string_view id, std::string new_id);
    bool reparent(std::string_view id, std::string_view parent);
    std::optional<AuthoringFragment> copy(std::string_view id) const;
    bool paste(const AuthoringFragment& fragment, std::string_view parent, std::string new_id);
    bool connect(std::string from, NavAction action, std::string to);
    bool disconnect(std::string_view from, NavAction action, std::string_view to);
    bool connect_groups(std::string from, NavAction action, std::string to);
    bool disconnect_groups(std::string_view from, NavAction action, std::string_view to);
    bool undo();
    bool redo();
    bool save();
    bool save_as(const std::filesystem::path& path);
    bool reload();
    std::string make_id(std::string_view prefix = "node");

  private:
    View view_;
    std::filesystem::path source_;
    std::string selection_;
    std::vector<View> undo_;
    std::vector<View> redo_;
    std::uint64_t next_id_ = 1;
    bool dirty_ = false;
};

} // namespace gview
