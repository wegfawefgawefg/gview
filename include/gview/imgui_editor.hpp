#pragma once

#include "gview/authoring.hpp"
#include "gview/compile.hpp"
#include "gview/paint.hpp"

#include <functional>
#include <optional>
#include <utility>

namespace gview {

enum class AuthoringMode { Test, Edit };

struct AuthoringHooks {
    std::vector<std::string> states;
    int active_state = 0;
    std::vector<std::string> scenarios;
    int active_scenario = 0;
    std::filesystem::path default_save_path;
    std::function<void(int)> select_state;
    std::function<void(int)> select_scenario;
    std::function<void(const PreviewConfig&)> apply_preview;
    std::function<std::pair<int, int>()> host_window_size;
    std::function<void(int, int)> resize_host_window;
    std::function<void()> rebuild;
    std::function<std::string()> metrics;
};

struct AuthoringUiState {
    PreviewConfig preview;
    glayout::GraphCanvasState canvas;
    AuthoringMode mode = AuthoringMode::Test;
    bool show_launcher = true;
    bool show_control = false;
    bool show_focus_graph = false;
    bool show_display = false;
    bool show_theme = false;
    bool focus_authoring = false;
    bool show_layout_boxes = false;
    bool show_focus_overlay = false;
    bool show_ids = false;
    bool show_grid = false;
    bool show_slice_guides = false;
    bool group_link_authoring = false;
    bool host_follows_logical = false;
    float host_logical_fraction = 1.0f;
    NavAction edge_action = NavAction::Right;
    std::string edge_source;
    std::string edge_target;
    std::string selected_focus_group;
    std::string slice_guide_asset;
    int selected_theme = 0;
    int selected_widget_skin = 0;
    int selected_widget_part = 0;
    std::optional<AuthoringFragment> clipboard;
    std::optional<View> transaction;
};

bool authoring_captures_runtime(const AuthoringUiState& state);

void draw_authoring_tools(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                          const CompiledView& compiled,
                          const std::vector<glayout::ResolvedNode>& geometry,
                          const std::vector<PaintCommand>* paint = nullptr);

} // namespace gview
