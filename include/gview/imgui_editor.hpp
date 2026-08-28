#pragma once

#include "gview/authoring.hpp"
#include "gview/compile.hpp"

#include <functional>
#include <optional>

namespace gview {

enum class AuthoringMode { Test, Edit };
enum class OverlayMode { Clean, Layout, Focus, Combined };

struct AuthoringHooks {
    std::vector<std::string> states;
    int active_state = 0;
    std::vector<std::string> scenarios;
    int active_scenario = 0;
    std::filesystem::path default_save_path;
    std::function<void(int)> select_state;
    std::function<void(int)> select_scenario;
    std::function<void(const PreviewConfig&)> apply_preview;
    std::function<void(int, int)> resize_output;
    std::function<void()> rebuild;
    std::function<std::string()> metrics;
};

struct AuthoringUiState {
    PreviewConfig preview;
    glayout::GraphCanvasState canvas;
    AuthoringMode mode = AuthoringMode::Test;
    OverlayMode overlay = OverlayMode::Combined;
    bool show_control = true;
    bool show_hierarchy = true;
    bool show_focus_graph = false;
    bool show_display = false;
    bool focus_authoring = false;
    bool show_ids = true;
    bool show_grid = true;
    bool group_link_authoring = false;
    NavAction edge_action = NavAction::Right;
    std::string edge_source;
    std::string edge_target;
    std::string selected_focus_group;
    std::optional<AuthoringFragment> clipboard;
    std::optional<View> transaction;
};

bool authoring_captures_runtime(const AuthoringUiState& state);

void draw_authoring_tools(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                          const CompiledView& compiled,
                          const std::vector<glayout::ResolvedNode>& geometry);

} // namespace gview
