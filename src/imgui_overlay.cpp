#include "imgui_editor_internal.hpp"

#include <imgui.h>

#include <algorithm>

namespace gview {
namespace {

ImVec2 mapped(float x, float y, float scale, ImVec2 origin) {
    return {origin.x + x * scale, origin.y + y * scale};
}

} // namespace

// Draws authored boxes over the native output and directly moves absolute
// children.
void draw_layout_overlay(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                         const CompiledView& compiled,
                         const std::vector<glayout::ResolvedNode>& geometry) {
    if (compiled.nodes.empty() || geometry.empty()) return;
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && state.transaction) {
        session.commit_snapshot(std::move(*state.transaction));
        state.transaction.reset();
    }
    const float scale = std::min(io.DisplaySize.x / static_cast<float>(state.preview.width),
                                 io.DisplaySize.y / static_cast<float>(state.preview.height));
    const ImVec2 origin{(io.DisplaySize.x - static_cast<float>(state.preview.width) * scale) * 0.5f,
                        (io.DisplaySize.y - static_cast<float>(state.preview.height) * scale) *
                            0.5f};
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 mouse = io.MousePos;
    NodeIndex hovered = invalid_node;
    for (NodeIndex reverse = static_cast<NodeIndex>(compiled.nodes.size()); reverse > 0;
         --reverse) {
        const NodeIndex index = reverse - 1;
        const glayout::Rect rect = geometry[compiled.nodes[index].layout_index].border;
        const ImVec2 min = mapped(rect.x, rect.y, scale, origin);
        const ImVec2 max = mapped(rect.x + rect.w, rect.y + rect.h, scale, origin);
        if (mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y) {
            hovered = index;
            break;
        }
    }
    if (!io.WantCaptureMouse && hovered != invalid_node &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        session.select(compiled.nodes[hovered].source.layout_id);

    for (NodeIndex index = 0; index < compiled.nodes.size(); ++index) {
        const glayout::Rect rect = geometry[compiled.nodes[index].layout_index].border;
        const ImVec2 min = mapped(rect.x, rect.y, scale, origin);
        const ImVec2 max = mapped(rect.x + rect.w, rect.y + rect.h, scale, origin);
        const bool selected = session.selection() == compiled.nodes[index].source.layout_id;
        draw->AddRect(min, max,
                      selected ? IM_COL32(154, 239, 117, 255) : IM_COL32(65, 185, 200, 100), 0.0f,
                      0, selected ? 2.0f : 1.0f);
        if (selected)
            draw->AddText({min.x + 3.0f, min.y + 3.0f}, IM_COL32(190, 255, 165, 255),
                          compiled.nodes[index].source.layout_id.c_str());
    }

    if (io.WantCaptureMouse || !ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
        session.selection().empty())
        return;
    glayout::GraphNode* node = glayout::find_graph_node(session.view().layout, session.selection());
    glayout::GraphNode* parent =
        glayout::find_graph_parent(session.view().layout, session.selection());
    if (!node || !parent ||
        (parent->container != glayout::ContainerKind::Absolute &&
         parent->container != glayout::ContainerKind::Overlay))
        return;
    if (!state.transaction) state.transaction = session.view();
    node->absolute_rect.x += io.MouseDelta.x / (static_cast<float>(state.preview.width) * scale);
    node->absolute_rect.y += io.MouseDelta.y / (static_cast<float>(state.preview.height) * scale);
    if (hooks.rebuild) hooks.rebuild();
}

} // namespace gview
