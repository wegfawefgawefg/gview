#include "imgui_editor_internal.hpp"

#include <imgui.h>

#include <array>
#include <cstring>

namespace gview {
namespace {

constexpr std::array<const char*, 6> container_names{"Absolute", "Row",   "Column",
                                                     "Grid",     "Stack", "Overlay"};
constexpr std::array<const char*, 5> length_names{"Auto", "Pixels", "Percent", "Fill", "Intrinsic"};
constexpr std::array<const char*, 6> content_names{"None",   "Text",     "Image",
                                                   "Sprite", "Progress", "Custom surface"};
constexpr std::array<const char*, 7> control_names{"None",   "Button",     "Toggle",     "Slider",
                                                   "Select", "Text input", "Scroll area"};
constexpr std::array<const char*, 4> align_names{"Start", "Center", "End", "Stretch"};
constexpr std::array<const char*, 4> distribution_names{"Start", "Center", "End", "Space between"};

void collect_node_ids(const glayout::GraphNode& node, std::vector<std::string>& ids) {
    ids.push_back(node.id);
    for (const glayout::GraphNode& child : node.children)
        collect_node_ids(child, ids);
}

bool edit_string(const char* label, std::string& value) {
    std::array<char, 256> buffer{};
    std::strncpy(buffer.data(), value.c_str(), buffer.size() - 1);
    if (!ImGui::InputText(label, buffer.data(), buffer.size())) return false;
    value = buffer.data();
    return true;
}

void mark_live_change(AuthoringUiState& state, AuthoringHooks& hooks, AuthoringSession& session,
                      View before, bool edited) {
    if (edited && !state.transaction) state.transaction = std::move(before);
    if (edited && hooks.rebuild) hooks.rebuild();
    if (ImGui::IsItemDeactivatedAfterEdit() && state.transaction) {
        session.commit_snapshot(std::move(*state.transaction));
        state.transaction.reset();
    }
}

void draw_tree(AuthoringSession& session, const glayout::GraphNode& node) {
    const bool selected = session.selection() == node.id;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (node.id == session.view().layout.root.id) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    const bool open = ImGui::TreeNodeEx(node.id.c_str(), flags, "%s", node.id.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) session.select(node.id);
    if (open) {
        for (const glayout::GraphNode& child : node.children)
            draw_tree(session, child);
        ImGui::TreePop();
    }
}

void draw_length(const char* label, glayout::Length& length, AuthoringSession& session,
                 AuthoringUiState& state, AuthoringHooks& hooks) {
    int kind = static_cast<int>(length.kind);
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo(label, &kind, length_names.data(), static_cast<int>(length_names.size()))) {
        const View before = session.view();
        length.kind = static_cast<glayout::LengthKind>(kind);
        session.commit_snapshot(before);
        if (hooks.rebuild) hooks.rebuild();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    const View before = session.view();
    const bool edited = ImGui::DragFloat((std::string("##") + label).c_str(), &length.value, 0.01f,
                                         0.0f, 4096.0f, "%.3f");
    mark_live_change(state, hooks, session, before, edited);
}

NodeSpec basic_spec(std::string id, ContentKind content, ControlKind control) {
    NodeSpec spec;
    spec.layout_id = std::move(id);
    spec.content = content;
    spec.control = control;
    spec.focusable = control != ControlKind::None;
    spec.text = content == ContentKind::Text ? "New element" : "";
    spec.style.normal.fill = {10, 28, 32, 220};
    spec.style.normal.border = {60, 110, 115, 255};
    spec.style.normal.border_width = 1.0f;
    spec.style.focused = spec.style.normal;
    spec.style.focused.border = {151, 239, 117, 255};
    return spec;
}

void add_node(AuthoringSession& session, ContentKind content, ControlKind control,
              AuthoringHooks& hooks) {
    const std::string id = session.make_id(control == ControlKind::Button ? "button" : "box");
    glayout::GraphNode node;
    node.id = id;
    node.size.width = {glayout::LengthKind::Pixels, 180.0f};
    node.size.height = {glayout::LengthKind::Pixels, 48.0f};
    node.absolute_rect = {0.1f, 0.1f, 0.25f, 0.08f};
    if (control == ControlKind::ScrollArea) {
        node.container = glayout::ContainerKind::Column;
        node.clip = true;
    }
    const std::string parent = glayout::find_graph_node(session.view().layout, session.selection())
                                   ? session.selection()
                                   : session.view().layout.root.id;
    if (session.add(parent, std::move(node), basic_spec(id, content, control))) {
        session.select(id);
        if (hooks.rebuild) hooks.rebuild();
    }
}

void draw_toolbar(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks) {
    if (ImGui::RadioButton("Test", state.mode == AuthoringMode::Test))
        state.mode = AuthoringMode::Test;
    ImGui::SameLine();
    if (ImGui::RadioButton("Edit canvas", state.mode == AuthoringMode::Edit))
        state.mode = AuthoringMode::Edit;
    ImGui::SameLine();
    ImGui::TextDisabled(state.mode == AuthoringMode::Edit ? "runtime input paused" : "UI is live");
    if (ImGui::Button("Undo") && session.undo() && hooks.rebuild) hooks.rebuild();
    ImGui::SameLine();
    if (ImGui::Button("Redo") && session.redo() && hooks.rebuild) hooks.rebuild();
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (!session.save()) session.save_as(hooks.default_save_path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload") && session.reload() && hooks.rebuild) hooks.rebuild();
    ImGui::Text("%s%s", session.source().empty() ? "C++ source" : session.source().c_str(),
                session.dirty() ? "  * modified" : "");

    ImGui::Checkbox("Layout boxes", &state.show_layout_boxes);
    ImGui::SameLine();
    ImGui::Checkbox("IDs", &state.show_ids);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &state.show_grid);
    ImGui::SameLine();
    ImGui::Checkbox("Focus overlay", &state.show_focus_overlay);
    ImGui::Checkbox("Display simulator", &state.show_display);
    ImGui::SameLine();
    ImGui::Checkbox("Focus inspector", &state.show_focus_graph);
}

void draw_launcher(AuthoringUiState& state) {
    if (!state.show_launcher) return;
    ImGui::SetNextWindowPos({12.0f, 34.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.96f);
    if (!ImGui::Begin("GView Tools  [F1]", &state.show_launcher,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    if (ImGui::RadioButton("Test [F2]", state.mode == AuthoringMode::Test))
        state.mode = AuthoringMode::Test;
    ImGui::SameLine();
    if (ImGui::RadioButton("Edit canvas [F2]", state.mode == AuthoringMode::Edit))
        state.mode = AuthoringMode::Edit;
    ImGui::TextDisabled(state.mode == AuthoringMode::Edit ? "runtime input paused"
                                                          : "game UI receives input");
    ImGui::SeparatorText("Native canvas");
    ImGui::Checkbox("Layout boxes [F3]", &state.show_layout_boxes);
    ImGui::Checkbox("Grid [F4]", &state.show_grid);
    ImGui::Checkbox("Focus graph [F5]", &state.show_focus_overlay);
    ImGui::Checkbox("Node IDs", &state.show_ids);
    if (ImGui::Button("Clean canvas")) {
        state.show_layout_boxes = false;
        state.show_grid = false;
        state.show_focus_overlay = false;
        state.show_ids = false;
    }
    ImGui::SeparatorText("Windows");
    ImGui::Checkbox("Hierarchy / properties", &state.show_control);
    ImGui::Checkbox("Display simulator", &state.show_display);
    ImGui::Checkbox("Focus inspector", &state.show_focus_graph);
    ImGui::TextDisabled("F1 hides this launcher; canvas editing stays active.");
    ImGui::End();
}

void draw_preview(AuthoringUiState& state, AuthoringHooks& hooks) {
    bool changed = false;
    if (!hooks.states.empty()) {
        const std::vector<const char*> labels = [&] {
            std::vector<const char*> result;
            for (const std::string& label : hooks.states)
                result.push_back(label.c_str());
            return result;
        }();
        if (ImGui::Combo("View / state", &hooks.active_state, labels.data(),
                         static_cast<int>(labels.size())) &&
            hooks.select_state)
            hooks.select_state(hooks.active_state);
    }
    if (!hooks.scenarios.empty()) {
        std::vector<const char*> labels;
        for (const std::string& label : hooks.scenarios)
            labels.push_back(label.c_str());
        if (ImGui::Combo("Data scenario", &hooks.active_scenario, labels.data(),
                         static_cast<int>(labels.size())) &&
            hooks.select_scenario)
            hooks.select_scenario(hooks.active_scenario);
    }
    if (changed && hooks.apply_preview) hooks.apply_preview(state.preview);
}

void draw_properties(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks) {
    glayout::GraphNode* node = glayout::find_graph_node(session.view().layout, session.selection());
    if (!node) return;
    ImGui::SeparatorText("Selected node");
    ImGui::TextUnformatted(node->id.c_str());
    std::vector<std::string> ids;
    collect_node_ids(session.view().layout.root, ids);
    const std::string current_parent = [&] {
        for (const std::string& id : ids) {
            const glayout::GraphNode* candidate =
                glayout::find_graph_node(session.view().layout, id);
            if (!candidate) continue;
            for (const glayout::GraphNode& child : candidate->children)
                if (child.id == node->id) return id;
        }
        return std::string{};
    }();
    if (!current_parent.empty() && ImGui::BeginCombo("Parent", current_parent.c_str())) {
        for (const std::string& id : ids) {
            if (id == node->id) continue;
            if (ImGui::Selectable(id.c_str(), id == current_parent)) {
                if (session.reparent(node->id, id) && hooks.rebuild) hooks.rebuild();
                ImGui::EndCombo();
                return;
            }
        }
        ImGui::EndCombo();
    }
    int container = static_cast<int>(node->container);
    if (ImGui::Combo("Container", &container, container_names.data(),
                     static_cast<int>(container_names.size()))) {
        const View before = session.view();
        node->container = static_cast<glayout::ContainerKind>(container);
        session.commit_snapshot(before);
        if (hooks.rebuild) hooks.rebuild();
    }
    draw_length("Width", node->size.width, session, state, hooks);
    draw_length("Height", node->size.height, session, state, hooks);
    View before = session.view();
    bool edited = ImGui::DragFloat("Min width", &node->size.min_width, 1.0f, 0.0f, 4096.0f);
    mark_live_change(state, hooks, session, before, edited);
    before = session.view();
    edited = ImGui::DragFloat("Min height", &node->size.min_height, 1.0f, 0.0f, 4096.0f);
    mark_live_change(state, hooks, session, before, edited);
    before = session.view();
    edited = ImGui::DragFloat("Aspect ratio", &node->size.aspect_ratio, 0.01f, 0.0f, 8.0f);
    mark_live_change(state, hooks, session, before, edited);
    before = session.view();
    edited = ImGui::DragInt("Grid columns", &node->columns, 0.1f, 1, 64);
    mark_live_change(state, hooks, session, before, edited);
    int align = static_cast<int>(node->align);
    if (ImGui::Combo("Align", &align, align_names.data(), static_cast<int>(align_names.size()))) {
        before = session.view();
        node->align = static_cast<glayout::Align>(align);
        session.commit_snapshot(before);
        if (hooks.rebuild) hooks.rebuild();
    }
    int distribution = static_cast<int>(node->distribution);
    if (ImGui::Combo("Distribution", &distribution, distribution_names.data(),
                     static_cast<int>(distribution_names.size()))) {
        before = session.view();
        node->distribution = static_cast<glayout::Distribution>(distribution);
        session.commit_snapshot(before);
        if (hooks.rebuild) hooks.rebuild();
    }
    before = session.view();
    edited = ImGui::Checkbox("Clip children", &node->clip);
    mark_live_change(state, hooks, session, before, edited);
    ImGui::SameLine();
    before = session.view();
    edited = ImGui::Checkbox("Visible", &node->visible);
    mark_live_change(state, hooks, session, before, edited);
    before = session.view();
    edited = ImGui::DragFloat("Gap", &node->gap, 0.5f, 0.0f, 128.0f);
    mark_live_change(state, hooks, session, before, edited);
    float padding[4]{node->padding.left, node->padding.top, node->padding.right,
                     node->padding.bottom};
    before = session.view();
    edited = ImGui::DragFloat4("Padding L/T/R/B", padding, 0.5f, 0.0f, 256.0f);
    if (edited) {
        node->padding = {padding[0], padding[1], padding[2], padding[3]};
    }
    mark_live_change(state, hooks, session, before, edited);
    float rect[4]{node->absolute_rect.x, node->absolute_rect.y, node->absolute_rect.w,
                  node->absolute_rect.h};
    before = session.view();
    edited = ImGui::DragFloat4("Absolute x/y/w/h", rect, 0.002f, -2.0f, 2.0f);
    if (edited) node->absolute_rect = {rect[0], rect[1], rect[2], rect[3]};
    mark_live_change(state, hooks, session, before, edited);

    const auto found =
        std::find_if(session.view().nodes.begin(), session.view().nodes.end(),
                     [&](const NodeSpec& item) { return item.layout_id == node->id; });
    if (found == session.view().nodes.end()) return;
    NodeSpec& spec = *found;
    ImGui::SeparatorText("Presentation / interaction");
    int content = static_cast<int>(spec.content);
    if (ImGui::Combo("Content", &content, content_names.data(),
                     static_cast<int>(content_names.size()))) {
        before = session.view();
        spec.content = static_cast<ContentKind>(content);
        session.commit_snapshot(before);
        if (hooks.rebuild) hooks.rebuild();
    }
    int control = static_cast<int>(spec.control);
    if (ImGui::Combo("Control", &control, control_names.data(),
                     static_cast<int>(control_names.size()))) {
        before = session.view();
        spec.control = static_cast<ControlKind>(control);
        spec.focusable = spec.control != ControlKind::None;
        session.commit_snapshot(before);
        if (hooks.rebuild) hooks.rebuild();
    }
    for (auto [label, value] : std::array<std::pair<const char*, std::string*>, 5>{
             {{"Text", &spec.text},
              {"Asset / surface", &spec.asset},
              {"Binding", &spec.binding},
              {"Action", &spec.action},
              {"Focus group", &spec.focus_group}}}) {
        before = session.view();
        if (edit_string(label, *value)) {
            session.commit_snapshot(before);
            if (hooks.rebuild) hooks.rebuild();
        }
    }
    before = session.view();
    edited = ImGui::DragFloat("Text size", &spec.text_style.size, 0.5f, 4.0f, 196.0f);
    mark_live_change(state, hooks, session, before, edited);
    before = session.view();
    edited = ImGui::DragFloat("Border width", &spec.style.normal.border_width, 0.25f, 0.0f, 24.0f);
    mark_live_change(state, hooks, session, before, edited);
    before = session.view();
    edited = ImGui::Checkbox("Focusable", &spec.focusable);
    mark_live_change(state, hooks, session, before, edited);
    ImGui::SameLine();
    before = session.view();
    edited = ImGui::Checkbox("Selected", &spec.selected);
    mark_live_change(state, hooks, session, before, edited);
}

} // namespace

// Presents reusable preview, hierarchy, property, graph, and overlay authoring
// surfaces.
void draw_authoring_tools(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                          const CompiledView& compiled,
                          const std::vector<glayout::ResolvedNode>& geometry) {
    draw_launcher(state);
    if (state.show_control) {
        ImGui::SetNextWindowPos({16.0f, 56.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({430.0f, 648.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.98f);
        if (ImGui::Begin("GView: Hierarchy & Properties", &state.show_control)) {
            draw_toolbar(session, state, hooks);
            if (ImGui::CollapsingHeader("View data", ImGuiTreeNodeFlags_DefaultOpen))
                draw_preview(state, hooks);
            if (hooks.metrics && ImGui::CollapsingHeader("Runtime performance"))
                ImGui::TextWrapped("%s", hooks.metrics().c_str());
            if (ImGui::Button("Add…")) ImGui::OpenPopup("add-node");
            if (ImGui::BeginPopup("add-node")) {
                if (ImGui::MenuItem("Container / box"))
                    add_node(session, ContentKind::None, ControlKind::None, hooks);
                if (ImGui::MenuItem("Text"))
                    add_node(session, ContentKind::Text, ControlKind::None, hooks);
                if (ImGui::MenuItem("Image / sprite"))
                    add_node(session, ContentKind::Image, ControlKind::None, hooks);
                if (ImGui::MenuItem("Button"))
                    add_node(session, ContentKind::Text, ControlKind::Button, hooks);
                if (ImGui::MenuItem("Toggle"))
                    add_node(session, ContentKind::Text, ControlKind::Toggle, hooks);
                if (ImGui::MenuItem("Slider"))
                    add_node(session, ContentKind::Text, ControlKind::Slider, hooks);
                if (ImGui::MenuItem("Select"))
                    add_node(session, ContentKind::Text, ControlKind::Select, hooks);
                if (ImGui::MenuItem("Text input"))
                    add_node(session, ContentKind::Text, ControlKind::TextInput, hooks);
                if (ImGui::MenuItem("Scroll area"))
                    add_node(session, ContentKind::None, ControlKind::ScrollArea, hooks);
                if (ImGui::MenuItem("Custom render surface"))
                    add_node(session, ContentKind::CustomSurface, ControlKind::None, hooks);
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Duplicate") &&
                session.duplicate(session.selection(), session.make_id("copy")) && hooks.rebuild)
                hooks.rebuild();
            ImGui::SameLine();
            if (ImGui::Button("Delete") && session.remove(session.selection()) && hooks.rebuild)
                hooks.rebuild();
            ImGui::SeparatorText("Hierarchy");
            draw_tree(session, session.view().layout.root);
            draw_properties(session, state, hooks);
        }
        ImGui::End();
    }
    draw_display_simulator(state, hooks);
    if (state.show_focus_graph) draw_focus_graph(session, state, hooks, compiled, geometry);
    if (state.show_layout_boxes || state.show_grid || state.show_focus_overlay ||
        state.mode == AuthoringMode::Edit)
        draw_layout_overlay(session, state, hooks, compiled, geometry);
}

bool authoring_captures_runtime(const AuthoringUiState& state) {
    return state.mode == AuthoringMode::Edit;
}

} // namespace gview
