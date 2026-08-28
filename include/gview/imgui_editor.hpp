#pragma once

#include "gview/authoring.hpp"
#include "gview/compile.hpp"

#include <functional>
#include <optional>

namespace gview {

struct AuthoringHooks {
    std::vector<std::string> states;
    int active_state = 0;
    std::vector<std::string> scenarios;
    int active_scenario = 0;
    std::filesystem::path default_save_path;
    std::function<void(int)> select_state;
    std::function<void(int)> select_scenario;
    std::function<void(const PreviewConfig&)> apply_preview;
    std::function<void()> rebuild;
    std::function<std::string()> metrics;
};

struct AuthoringUiState {
    PreviewConfig preview;
    bool show_control = true;
    bool show_hierarchy = true;
    bool show_focus_graph = true;
    bool show_overlay = true;
    NavAction edge_action = NavAction::Right;
    std::string edge_source;
    std::optional<View> transaction;
};

void draw_authoring_tools(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                          const CompiledView& compiled,
                          const std::vector<glayout::ResolvedNode>& geometry);

} // namespace gview
