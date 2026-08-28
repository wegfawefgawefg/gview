#include "gview/focus.hpp"
#include "imgui_editor_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace gview {
namespace {

ImVec2 map_point(glayout::Rect rect, ImVec2 origin, ImVec2 extent, int width, int height) {
    return {origin.x + rect.x / static_cast<float>(width) * extent.x,
            origin.y + rect.y / static_cast<float>(height) * extent.y};
}

const char* action_name(NavAction action) { return to_string(action).data(); }

} // namespace

// Renders explicit directed focus edges and supports source-then-target edge
// authoring.
void draw_focus_graph(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                      const CompiledView& compiled,
                      const std::vector<glayout::ResolvedNode>& geometry) {
    ImGui::SetNextWindowPos({810.0f, 56.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({454.0f, 648.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.98f);
    if (!ImGui::Begin("GView: Focus Graph", &state.show_focus_graph)) {
        ImGui::End();
        return;
    }
    constexpr std::array<NavAction, 8> actions{
        NavAction::Up,      NavAction::Down, NavAction::Left,        NavAction::Right,
        NavAction::Confirm, NavAction::Back, NavAction::TabPrevious, NavAction::TabNext};
    int selected_action = 0;
    for (std::size_t index = 0; index < actions.size(); ++index)
        if (actions[index] == state.edge_action) selected_action = static_cast<int>(index);
    constexpr const char* labels[]{"Up",      "Down", "Left",         "Right",
                                   "Confirm", "Back", "Tab previous", "Tab next"};
    if (ImGui::Combo("Direction", &selected_action, labels, 8))
        state.edge_action = actions[static_cast<std::size_t>(selected_action)];
    ImGui::SameLine();
    if (ImGui::Button("Clear source")) state.edge_source.clear();
    ImGui::Text("Click source, then target. Source: %s",
                state.edge_source.empty() ? "none" : state.edge_source.c_str());
    if (ImGui::CollapsingHeader("Explicit overrides")) {
        for (std::size_t index = 0; index < session.view().focus_edges.size(); ++index) {
            const FocusEdge edge = session.view().focus_edges[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::Text("%s  --%s-->  %s", edge.from.c_str(), action_name(edge.action),
                        edge.to.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                if (session.disconnect(edge.from, edge.action, edge.to) && hooks.rebuild)
                    hooks.rebuild();
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
    const std::vector<FocusIssue> issues = analyze_focus_graph(compiled, geometry);
    if (!issues.empty()) {
        ImGui::TextColored({1.0f, 0.48f, 0.42f, 1.0f}, "%zu focus issue(s)", issues.size());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            for (const FocusIssue& issue : issues)
                ImGui::TextUnformatted(issue.message.c_str());
            ImGui::EndTooltip();
        }
    }

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 extent{ImGui::GetContentRegionAvail().x,
                        std::max(260.0f, ImGui::GetContentRegionAvail().y)};
    ImGui::InvisibleButton("focus-canvas", extent);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + extent.x, origin.y + extent.y},
                        IM_COL32(4, 15, 18, 255));

    const int width = std::max(1, static_cast<int>(geometry.front().border.w));
    const int height = std::max(1, static_cast<int>(geometry.front().border.h));
    for (const CompiledFocusEdge& edge : compiled.focus_edges) {
        const glayout::Rect from = geometry[compiled.nodes[edge.from].layout_index].border;
        const glayout::Rect to = geometry[compiled.nodes[edge.to].layout_index].border;
        ImVec2 start = map_point({from.x + from.w * 0.5f, from.y + from.h * 0.5f, 0, 0}, origin,
                                 extent, width, height);
        ImVec2 end = map_point({to.x + to.w * 0.5f, to.y + to.h * 0.5f, 0, 0}, origin, extent,
                               width, height);
        draw->AddLine(start, end, IM_COL32(71, 190, 210, 190), 1.5f);
        const ImVec2 middle{(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
        draw->AddText(middle, IM_COL32(150, 230, 239, 255), action_name(edge.action));
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    NodeIndex clicked = invalid_node;
    for (NodeIndex index = 0; index < compiled.nodes.size(); ++index) {
        if (!compiled.nodes[index].source.focusable) continue;
        const glayout::Rect rect = geometry[compiled.nodes[index].layout_index].border;
        const ImVec2 min = map_point(rect, origin, extent, width, height);
        const ImVec2 max =
            map_point({rect.x + rect.w, rect.y + rect.h, 0, 0}, origin, extent, width, height);
        const bool selected = session.selection() == compiled.nodes[index].source.layout_id;
        const bool source = state.edge_source == compiled.nodes[index].source.layout_id;
        const bool unreachable =
            std::any_of(issues.begin(), issues.end(),
                        [&](const FocusIssue& issue) { return issue.node == index; });
        draw->AddRectFilled(min, max,
                            source ? IM_COL32(50, 100, 60, 240) : IM_COL32(10, 35, 40, 220));
        draw->AddRect(min, max,
                      unreachable ? IM_COL32(255, 90, 75, 255)
                      : selected  ? IM_COL32(180, 255, 150, 255)
                                  : IM_COL32(70, 130, 135, 255));
        draw->AddText({min.x + 3.0f, min.y + 2.0f}, IM_COL32(225, 238, 235, 255),
                      compiled.nodes[index].source.layout_id.c_str());
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y)
            clicked = index;
    }
    if (clicked != invalid_node) {
        const std::string id = compiled.nodes[clicked].source.layout_id;
        session.select(id);
        if (state.edge_source.empty()) state.edge_source = id;
        else if (state.edge_source != id) {
            if (session.connect(state.edge_source, state.edge_action, id) && hooks.rebuild)
                hooks.rebuild();
            state.edge_source.clear();
        }
    }
    ImGui::End();
}

} // namespace gview
