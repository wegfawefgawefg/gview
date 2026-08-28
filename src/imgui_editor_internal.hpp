#pragma once

#include "gview/imgui_editor.hpp"

namespace gview {

void draw_focus_graph(AuthoringSession& session, AuthoringUiState& state,
                      AuthoringHooks& hooks, const CompiledView& compiled,
                      const std::vector<glayout::ResolvedNode>& geometry);
void draw_display_simulator(AuthoringUiState& state, AuthoringHooks& hooks);
void draw_layout_overlay(AuthoringSession& session, AuthoringUiState& state,
                         AuthoringHooks& hooks, const CompiledView& compiled,
                         const std::vector<glayout::ResolvedNode>& geometry);

} // namespace gview
