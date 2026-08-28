#include "gview/focus.hpp"
#include "imgui_editor_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>

namespace gview {
namespace {

constexpr std::array<NavAction, 8> actions{
    NavAction::Up,      NavAction::Down, NavAction::Left,        NavAction::Right,
    NavAction::Confirm, NavAction::Back, NavAction::TabPrevious, NavAction::TabNext};
constexpr std::array<const char*, 8> labels{
    "Up", "Down", "Left", "Right", "Confirm", "Back", "Tab previous", "Tab next"};

FocusGroup* selected_group(AuthoringSession& session, const AuthoringUiState& state) {
    const auto found = std::find_if(session.view().focus_groups.begin(),
                                    session.view().focus_groups.end(), [&](const FocusGroup& group) {
                                        return group.id == state.selected_focus_group;
                                    });
    return found == session.view().focus_groups.end() ? nullptr : &*found;
}

std::string node_group(const AuthoringSession& session, std::string_view node_id) {
    const auto found = std::find_if(session.view().nodes.begin(), session.view().nodes.end(),
                                    [&](const NodeSpec& node) {
                                        return node.layout_id == node_id;
                                    });
    return found == session.view().nodes.end() ? std::string{} : found->focus_group;
}

void group_editor(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks) {
    if (ImGui::BeginCombo("Focus group", state.selected_focus_group.empty()
                                            ? "None selected"
                                            : state.selected_focus_group.c_str())) {
        for (const FocusGroup& group : session.view().focus_groups)
            if (ImGui::Selectable(group.id.c_str(), group.id == state.selected_focus_group))
                state.selected_focus_group = group.id;
        ImGui::EndCombo();
    }
    if (ImGui::Button("New group")) {
        const std::string id = session.make_id("group");
        const std::string selected = session.selection();
        if (session.edit([&](View& view) {
                view.focus_groups.push_back({id, selected, selected, false, true});
                return true;
            })) {
            state.selected_focus_group = id;
            if (hooks.rebuild) hooks.rebuild();
        }
    }
    FocusGroup* group = selected_group(session, state);
    if (!group) return;
    ImGui::SameLine();
    if (ImGui::Button("Delete group")) {
        const std::string id = group->id;
        if (session.edit([&](View& view) {
                std::erase_if(view.focus_groups,
                              [&](const FocusGroup& item) { return item.id == id; });
                std::erase_if(view.focus_group_edges, [&](const FocusGroupEdge& edge) {
                    return edge.from == id || edge.to == id;
                });
                for (NodeSpec& node : view.nodes)
                    if (node.focus_group == id) node.focus_group.clear();
                return true;
            }) && hooks.rebuild)
            hooks.rebuild();
        state.selected_focus_group.clear();
        return;
    }
    ImGui::Text("Owner: %s", group->owner.empty() ? "—" : group->owner.c_str());
    ImGui::Text("Entry: %s", group->entry.empty() ? "—" : group->entry.c_str());
    if (ImGui::Button("Selected → owner")) {
        const std::string id = group->id;
        const std::string selected = session.selection();
        if (session.edit([&](View& view) {
                for (FocusGroup& item : view.focus_groups)
                    if (item.id == id) item.owner = selected;
                return true;
            }) && hooks.rebuild)
            hooks.rebuild();
    }
    ImGui::SameLine();
    if (ImGui::Button("Selected → entry")) {
        const std::string id = group->id;
        const std::string selected = session.selection();
        if (session.edit([&](View& view) {
                for (FocusGroup& item : view.focus_groups)
                    if (item.id == id) item.entry = selected;
                return true;
            }) && hooks.rebuild)
            hooks.rebuild();
    }
    if (ImGui::Button("Assign selected to group")) {
        const std::string id = group->id;
        const std::string selected = session.selection();
        if (session.edit([&](View& view) {
                for (NodeSpec& node : view.nodes)
                    if (node.layout_id == selected) node.focus_group = id;
                return true;
            }) && hooks.rebuild)
            hooks.rebuild();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove selected from group")) {
        const std::string selected = session.selection();
        if (session.edit([&](View& view) {
                for (NodeSpec& node : view.nodes)
                    if (node.layout_id == selected) node.focus_group.clear();
                return true;
            }) && hooks.rebuild)
            hooks.rebuild();
    }
    const bool contain = group->contain;
    const bool remember = group->remember;
    bool next_contain = contain;
    bool next_remember = remember;
    const bool contain_changed = ImGui::Checkbox("Contain directional focus", &next_contain);
    const bool remember_changed = ImGui::Checkbox("Remember last child", &next_remember);
    if (contain_changed || remember_changed) {
        const std::string id = group->id;
        if (session.edit([&](View& view) {
                for (FocusGroup& item : view.focus_groups)
                    if (item.id == id) {
                        item.contain = next_contain;
                        item.remember = next_remember;
                    }
                return true;
            }) && hooks.rebuild)
            hooks.rebuild();
    }
}

} // namespace

// Keeps focus authoring deliberate: the inspector arms an operation while the
// native canvas supplies spatially truthful source and target selection.
void draw_focus_graph(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                      const CompiledView& compiled,
                      const std::vector<glayout::ResolvedNode>& geometry) {
    ImGui::SetNextWindowPos({810.0f, 56.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({454.0f, 430.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("GView: Focus Inspector", &state.show_focus_graph)) {
        ImGui::End();
        return;
    }
    int selected = 0;
    for (std::size_t index = 0; index < actions.size(); ++index)
        if (actions[index] == state.edge_action) selected = static_cast<int>(index);
    if (ImGui::Combo("Direction / event", &selected, labels.data(),
                     static_cast<int>(labels.size())))
        state.edge_action = actions[static_cast<std::size_t>(selected)];
    if (ImGui::Checkbox("Connect focus groups", &state.group_link_authoring)) {
        state.edge_source.clear();
        state.edge_target.clear();
    }

    if (ImGui::Button(state.focus_authoring ? "Cancel link authoring" : "Arm native-canvas link")) {
        state.focus_authoring = !state.focus_authoring;
        state.edge_source.clear();
        state.edge_target.clear();
        if (state.focus_authoring) {
            state.mode = AuthoringMode::Edit;
            state.overlay = OverlayMode::Combined;
        }
    }
    ImGui::TextWrapped("Click source and target on the rendered UI, then apply. Runtime input is "
                       "paused while canvas editing is active.");
    const std::string source_group = node_group(session, state.edge_source);
    const std::string target_group = node_group(session, state.edge_target);
    ImGui::Text("Source: %s", state.edge_source.empty() ? "—" : state.edge_source.c_str());
    if (state.group_link_authoring) {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", source_group.empty() ? "no group" : source_group.c_str());
    }
    ImGui::Text("Target: %s", state.edge_target.empty() ? "—" : state.edge_target.c_str());
    if (state.group_link_authoring) {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", target_group.empty() ? "no group" : target_group.c_str());
    }
    const bool ready = !state.edge_source.empty() && !state.edge_target.empty() &&
                       (!state.group_link_authoring ||
                        (!source_group.empty() && !target_group.empty() &&
                         source_group != target_group));
    ImGui::BeginDisabled(!ready);
    if (ImGui::Button("Apply link")) {
        const bool changed = state.group_link_authoring
                                 ? session.connect_groups(source_group, state.edge_action,
                                                          target_group)
                                 : session.connect(state.edge_source, state.edge_action,
                                                   state.edge_target);
        if (changed && hooks.rebuild)
            hooks.rebuild();
        state.edge_source.clear();
        state.edge_target.clear();
        state.focus_authoring = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Clear staged")) {
        state.edge_source.clear();
        state.edge_target.clear();
    }

    const std::vector<FocusIssue> issues = analyze_focus_graph(compiled, geometry);
    if (issues.empty()) ImGui::TextColored({0.55f, 0.95f, 0.45f, 1.0f}, "No reachability issues");
    else {
        ImGui::TextColored({1.0f, 0.48f, 0.42f, 1.0f}, "%zu reachability issue(s)",
                           issues.size());
        for (const FocusIssue& issue : issues)
            ImGui::BulletText("%s", issue.message.c_str());
    }

    if (ImGui::CollapsingHeader("Explicit overrides", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (std::size_t index = 0; index < session.view().focus_edges.size(); ++index) {
            const FocusEdge edge = session.view().focus_edges[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TextWrapped("%s  --%s-->  %s", edge.from.c_str(), to_string(edge.action).data(),
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
        for (std::size_t index = 0; index < session.view().focus_group_edges.size(); ++index) {
            const FocusGroupEdge edge = session.view().focus_group_edges[index];
            ImGui::PushID(static_cast<int>(session.view().focus_edges.size() + index));
            ImGui::TextWrapped("[%s]  --%s-->  [%s]", edge.from.c_str(),
                               to_string(edge.action).data(), edge.to.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                if (session.disconnect_groups(edge.from, edge.action, edge.to) && hooks.rebuild)
                    hooks.rebuild();
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Groups, owners, entry, and memory",
                                ImGuiTreeNodeFlags_DefaultOpen))
        group_editor(session, state, hooks);
    ImGui::End();
}

} // namespace gview
