#include "imgui_editor_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace gview {
namespace {

struct CanvasMapping {
    float scale = 1.0f;
    ImVec2 origin;
};

struct BannerRect {
    ImVec2 min;
    ImVec2 max;
};

struct BannerStack {
    ImVec2 min;
    ImVec2 max;
    std::vector<BannerRect> occupied;
};

CanvasMapping canvas_mapping(const AuthoringUiState& state, const ImGuiIO& io) {
    const float fit = std::min(io.DisplaySize.x / static_cast<float>(state.preview.width),
                               io.DisplaySize.y / static_cast<float>(state.preview.height));
    const float scale = fit * state.preview.zoom;
    return {scale,
            {(io.DisplaySize.x - static_cast<float>(state.preview.width) * scale) * 0.5f +
                 state.preview.pan_x,
             (io.DisplaySize.y - static_cast<float>(state.preview.height) * scale) * 0.5f +
                 state.preview.pan_y}};
}

ImVec2 mapped(float x, float y, const CanvasMapping& map) {
    return {map.origin.x + x * map.scale, map.origin.y + y * map.scale};
}

std::pair<float, float> logical_point(ImVec2 point, const CanvasMapping& map) {
    return {(point.x - map.origin.x) / map.scale, (point.y - map.origin.y) / map.scale};
}

NodeIndex hit_focus_node(const CompiledView& view,
                         const std::vector<glayout::ResolvedNode>& geometry, float x, float y) {
    for (NodeIndex reverse = static_cast<NodeIndex>(view.nodes.size()); reverse > 0; --reverse) {
        const NodeIndex index = reverse - 1;
        if (!view.nodes[index].source.focusable) continue;
        const glayout::Rect rect = geometry[view.nodes[index].layout_index].border;
        if (x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h)
            return index;
    }
    return invalid_node;
}

void draw_grid(ImDrawList* draw, const AuthoringUiState& state, const CanvasMapping& map) {
    if (!state.show_grid || state.canvas.grid_step <= 0.0f) return;
    const float step = state.canvas.grid_step * map.scale;
    if (step < 4.0f) return;
    const ImVec2 end = mapped(static_cast<float>(state.preview.width),
                              static_cast<float>(state.preview.height), map);
    for (float x = map.origin.x; x <= end.x; x += step)
        draw->AddLine({x, map.origin.y}, {x, end.y}, IM_COL32(80, 135, 140, 34));
    for (float y = map.origin.y; y <= end.y; y += step)
        draw->AddLine({map.origin.x, y}, {end.x, y}, IM_COL32(80, 135, 140, 34));
}

void fit_pair(float& first, float& second, float extent) {
    const float total = first + second;
    if (total <= extent || total <= 0.0f) return;
    const float scale = extent / total;
    first *= scale;
    second *= scale;
}

// Projects the selected asset's rendered slice boundaries over every live use.
void draw_slice_guides(ImDrawList* draw, const AuthoringUiState& state, const CanvasMapping& map,
                       const std::vector<PaintCommand>* paint) {
    if (!state.show_slice_guides || !paint) return;
    for (const PaintCommand& command : *paint) {
        if (command.kind != PaintKind::Image || command.image_mode != ImageMode::NineSlice)
            continue;
        if (!state.slice_guide_asset.empty() && command.asset != state.slice_guide_asset) continue;
        const auto margin = [&](float value) {
            return std::max(0.0f, value >= 0.0f ? value : command.slice) *
                   std::max(0.0f, command.slice_scale);
        };
        float left = margin(command.slice_margins.left);
        float top = margin(command.slice_margins.top);
        float right = margin(command.slice_margins.right);
        float bottom = margin(command.slice_margins.bottom);
        fit_pair(left, right, command.rect.w);
        fit_pair(top, bottom, command.rect.h);
        const ImVec2 min = mapped(command.rect.x, command.rect.y, map);
        const ImVec2 max =
            mapped(command.rect.x + command.rect.w, command.rect.y + command.rect.h, map);
        const ImU32 color = IM_COL32(255, 196, 74, 235);
        draw->AddRect(min, max, color, 0.0f, 0, 1.5f);
        draw->AddLine(mapped(command.rect.x + left, command.rect.y, map),
                      mapped(command.rect.x + left, command.rect.y + command.rect.h, map), color);
        draw->AddLine(
            mapped(command.rect.x + command.rect.w - right, command.rect.y, map),
            mapped(command.rect.x + command.rect.w - right, command.rect.y + command.rect.h, map),
            color);
        draw->AddLine(mapped(command.rect.x, command.rect.y + top, map),
                      mapped(command.rect.x + command.rect.w, command.rect.y + top, map), color);
        draw->AddLine(
            mapped(command.rect.x, command.rect.y + command.rect.h - bottom, map),
            mapped(command.rect.x + command.rect.w, command.rect.y + command.rect.h - bottom, map),
            color);
    }
}

void draw_handles(ImDrawList* draw, glayout::Rect bounds, const CanvasMapping& map) {
    const ImVec2 min = mapped(bounds.x, bounds.y, map);
    const ImVec2 max = mapped(bounds.x + bounds.w, bounds.y + bounds.h, map);
    const ImVec2 middle{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f};
    constexpr float half = 4.0f;
    const ImVec2 points[]{{min.x, min.y},    {middle.x, min.y}, {max.x, min.y},
                          {min.x, middle.y}, {max.x, middle.y}, {min.x, max.y},
                          {middle.x, max.y}, {max.x, max.y}};
    for (ImVec2 point : points) {
        draw->AddRectFilled({point.x - half, point.y - half}, {point.x + half, point.y + half},
                            IM_COL32(7, 22, 26, 255));
        draw->AddRect({point.x - half, point.y - half}, {point.x + half, point.y + half},
                      IM_COL32(190, 255, 165, 255));
    }
}

bool focus_group_bounds(const CompiledView& compiled,
                        const std::vector<glayout::ResolvedNode>& geometry,
                        std::string_view group_id, glayout::Rect& bounds) {
    bool found = false;
    for (const CompiledNode& node : compiled.nodes) {
        if (node.source.focus_group != group_id) continue;
        const glayout::Rect rect = geometry[node.layout_index].border;
        if (!found)
            bounds = rect;
        else {
            const float left = std::min(bounds.x, rect.x);
            const float top = std::min(bounds.y, rect.y);
            const float right = std::max(bounds.x + bounds.w, rect.x + rect.w);
            const float bottom = std::max(bounds.y + bounds.h, rect.y + rect.h);
            bounds = {left, top, right - left, bottom - top};
        }
        found = true;
    }
    return found;
}

bool overlaps(BannerRect left, BannerRect right) {
    constexpr float clearance = 4.0f;
    return left.min.x < right.max.x + clearance && left.max.x + clearance > right.min.x &&
           left.min.y < right.max.y + clearance && left.max.y + clearance > right.min.y;
}

BannerRect banner_rect(ImVec2 center, ImVec2 text_size) {
    return {{center.x - text_size.x * 0.5f - 5.0f, center.y - text_size.y * 0.5f - 3.0f},
            {center.x + text_size.x * 0.5f + 5.0f, center.y + text_size.y * 0.5f + 3.0f}};
}

void reserve_text(BannerStack& stack, ImVec2 position, const std::string& label) {
    const ImVec2 size = ImGui::CalcTextSize(label.c_str());
    stack.occupied.push_back({position, {position.x + size.x, position.y + size.y}});
}

// Stacks colliding relationship banners into nearby lanes while retaining a
// leader to the relationship's truthful midpoint.
void draw_link_label(ImDrawList* draw, BannerStack& stack, ImVec2 anchor, ImU32 color,
                     const std::string& label) {
    const ImVec2 size = ImGui::CalcTextSize(label.c_str());
    const float half_width = size.x * 0.5f + 5.0f;
    const float half_height = size.y * 0.5f + 3.0f;
    const float minimum_x = stack.min.x + half_width + 4.0f;
    const float maximum_x = stack.max.x - half_width - 4.0f;
    ImVec2 center{minimum_x <= maximum_x ? std::clamp(anchor.x, minimum_x, maximum_x) : anchor.x,
                  anchor.y};
    BannerRect chosen = banner_rect(center, size);
    constexpr float lane_height = 24.0f;
    for (int lane = 0; lane < 16; ++lane) {
        const int distance = (lane + 1) / 2;
        const float direction = lane == 0 || lane % 2 == 1 ? -1.0f : 1.0f;
        center.y = std::clamp(anchor.y + direction * static_cast<float>(distance) * lane_height,
                              stack.min.y + half_height + 4.0f, stack.max.y - half_height - 4.0f);
        chosen = banner_rect(center, size);
        if (std::none_of(stack.occupied.begin(), stack.occupied.end(),
                         [&](BannerRect item) { return overlaps(chosen, item); }))
            break;
    }
    stack.occupied.push_back(chosen);
    if (std::fabs(center.x - anchor.x) > 1.0f || std::fabs(center.y - anchor.y) > 1.0f)
        draw->AddLine(anchor, center, color, 1.0f);
    draw->AddRectFilled(chosen.min, chosen.max, IM_COL32(4, 15, 18, 238), 2.0f);
    draw->AddRect(chosen.min, chosen.max, color, 2.0f);
    draw->AddText({chosen.min.x + 5.0f, chosen.min.y + 3.0f}, color, label.c_str());
}

void draw_focus_edges(ImDrawList* draw, const CompiledView& compiled,
                      const std::vector<glayout::ResolvedNode>& geometry,
                      const AuthoringUiState& state, const CanvasMapping& map) {
    BannerStack banners{map.origin,
                        mapped(static_cast<float>(state.preview.width),
                               static_cast<float>(state.preview.height), map),
                        {}};
    for (const CompiledFocusGroup& group : compiled.focus_groups) {
        glayout::Rect bounds;
        if (!focus_group_bounds(compiled, geometry, group.id, bounds)) continue;
        const ImVec2 min = mapped(bounds.x - 4.0f, bounds.y - 18.0f, map);
        const ImVec2 max = mapped(bounds.x + bounds.w + 4.0f, bounds.y + bounds.h + 4.0f, map);
        draw->AddRect(min, max, IM_COL32(242, 113, 177, 190), 0.0f, 0, 1.5f);
        const std::string label = group.id + (group.remember ? " · remembers last item" : "");
        const ImVec2 position{min.x + 3.0f, min.y + 2.0f};
        draw->AddText(position, IM_COL32(255, 170, 215, 245), label.c_str());
        reserve_text(banners, position, label);
    }
    for (const CompiledFocusEdge& edge : compiled.focus_edges) {
        const glayout::Rect from = geometry[compiled.nodes[edge.from].layout_index].border;
        const glayout::Rect to = geometry[compiled.nodes[edge.to].layout_index].border;
        const ImVec2 start = mapped(from.x + from.w * 0.5f, from.y + from.h * 0.5f, map);
        const ImVec2 end = mapped(to.x + to.w * 0.5f, to.y + to.h * 0.5f, map);
        draw->AddLine(start, end, IM_COL32(85, 214, 230, 205), 2.0f);
        draw->AddCircleFilled(end, 3.5f, IM_COL32(150, 240, 245, 255));
        const ImVec2 center{(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
        const std::string label = compiled.nodes[edge.from].source.layout_id + "  --" +
                                  std::string(to_string(edge.action)) + "-->  " +
                                  compiled.nodes[edge.to].source.layout_id;
        draw_link_label(draw, banners, {center.x, center.y - 10.0f}, IM_COL32(180, 245, 248, 255),
                        label);
    }
    for (const CompiledFocusGroupEdge& edge : compiled.focus_group_edges) {
        glayout::Rect from;
        glayout::Rect to;
        if (!focus_group_bounds(compiled, geometry, edge.from, from) ||
            !focus_group_bounds(compiled, geometry, edge.to, to))
            continue;
        const ImVec2 start = mapped(from.x + from.w * 0.5f, from.y + from.h * 0.5f, map);
        const ImVec2 end = mapped(to.x + to.w * 0.5f, to.y + to.h * 0.5f, map);
        draw->AddLine(start, end, IM_COL32(242, 113, 177, 220), 3.0f);
        draw->AddCircleFilled(end, 4.0f, IM_COL32(255, 170, 215, 255));
        const ImVec2 center{(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
        const std::string label =
            edge.from + "  --" + std::string(to_string(edge.action)) + "-->  " + edge.to;
        draw_link_label(draw, banners, {center.x, center.y - 10.0f}, IM_COL32(255, 190, 225, 255),
                        label);
    }
    if (!state.edge_source.empty()) {
        const auto found = compiled.indices.find(state.edge_source);
        if (found != compiled.indices.end()) {
            const glayout::Rect rect = geometry[compiled.nodes[found->second].layout_index].border;
            const ImVec2 min = mapped(rect.x, rect.y, map);
            const ImVec2 max = mapped(rect.x + rect.w, rect.y + rect.h, map);
            draw->AddRect(min, max, IM_COL32(255, 200, 80, 255), 0.0f, 0, 3.0f);
        }
    }
    const ImVec2 legend = mapped(12.0f, 82.0f, map);
    draw_link_label(draw, banners, {legend.x + 255.0f, legend.y}, IM_COL32(210, 230, 232, 245),
                    "cyan: item override   pink: scope transition   inside scope: spatial");
}

} // namespace

// Draws and manipulates the real rendered canvas so polished assets, resolved
// geometry, snapping, grouping, and focus links are authored in one context.
void draw_layout_overlay(AuthoringSession& session, AuthoringUiState& state, AuthoringHooks& hooks,
                         const CompiledView& compiled,
                         const std::vector<glayout::ResolvedNode>& geometry,
                         const std::vector<PaintCommand>* paint) {
    if (compiled.nodes.empty() || geometry.empty()) return;
    const ImGuiIO& io = ImGui::GetIO();
    const CanvasMapping map = canvas_mapping(state, io);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw_grid(draw, state, map);
    draw_slice_guides(draw, state, map, paint);

    if (state.show_layout_boxes || state.show_ids || state.mode == AuthoringMode::Edit) {
        for (NodeIndex index = 0; index < compiled.nodes.size(); ++index) {
            const glayout::Rect rect = geometry[compiled.nodes[index].layout_index].border;
            const ImVec2 min = mapped(rect.x, rect.y, map);
            const ImVec2 max = mapped(rect.x + rect.w, rect.y + rect.h, map);
            const bool selected =
                std::find(state.canvas.selection.begin(), state.canvas.selection.end(),
                          compiled.nodes[index].source.layout_id) != state.canvas.selection.end();
            if (state.show_layout_boxes || selected)
                draw->AddRect(min, max,
                              selected ? IM_COL32(154, 239, 117, 255) : IM_COL32(65, 185, 200, 105),
                              0.0f, 0, selected ? 2.0f : 1.0f);
            if (state.show_ids)
                draw->AddText({min.x + 3.0f, min.y + 2.0f}, IM_COL32(190, 245, 235, 220),
                              compiled.nodes[index].source.layout_id.c_str());
        }
        glayout::Rect bounds;
        if (state.mode == AuthoringMode::Edit &&
            glayout::graph_canvas_selection_bounds(compiled.layout, geometry, state.canvas, bounds))
            draw_handles(draw, bounds, map);
        for (const glayout::CanvasGuide& guide : state.canvas.guides) {
            if (guide.vertical) {
                const float x = mapped(guide.position, 0.0f, map).x;
                draw->AddLine({x, map.origin.y}, {x, io.DisplaySize.y},
                              IM_COL32(255, 110, 190, 230), 1.5f);
            } else {
                const float y = mapped(0.0f, guide.position, map).y;
                draw->AddLine({map.origin.x, y}, {io.DisplaySize.x, y},
                              IM_COL32(255, 110, 190, 230), 1.5f);
            }
        }
    }
    if (state.show_focus_overlay) draw_focus_edges(draw, compiled, geometry, state, map);
    if (state.mode != AuthoringMode::Edit) return;

    if (state.show_control && state.canvas.selection.empty() && !session.selection().empty()) {
        state.canvas.selection = {session.selection()};
        state.canvas.primary = session.selection();
    }

    const bool command = io.KeyCtrl || io.KeySuper;
    if (!io.WantTextInput && command && ImGui::IsKeyPressed(ImGuiKey_C, false))
        state.clipboard = session.copy(session.selection());
    if (!io.WantTextInput && command && ImGui::IsKeyPressed(ImGuiKey_X, false)) {
        state.clipboard = session.copy(session.selection());
        if (state.clipboard && session.remove(session.selection()) && hooks.rebuild)
            hooks.rebuild();
        glayout::graph_canvas_clear(state.canvas);
    }
    if (!io.WantTextInput && command && ImGui::IsKeyPressed(ImGuiKey_V, false) && state.clipboard) {
        const std::string parent =
            session.selection().empty() ? session.view().layout.root.id : session.selection();
        if (session.paste(*state.clipboard, parent, session.make_id("paste")) && hooks.rebuild)
            hooks.rebuild();
    }
    if (!io.WantTextInput && command && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        const bool changed = io.KeyShift ? session.redo() : session.undo();
        if (changed && hooks.rebuild) hooks.rebuild();
    }
    if (!io.WantTextInput && command && ImGui::IsKeyPressed(ImGuiKey_Y, false) && session.redo() &&
        hooks.rebuild)
        hooks.rebuild();
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
        session.remove(session.selection()) && hooks.rebuild)
        hooks.rebuild();

    float nudge_x = 0.0f;
    float nudge_y = 0.0f;
    const float nudge_step = io.KeyShift ? state.canvas.grid_step : 1.0f;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) nudge_x -= nudge_step;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) nudge_x += nudge_step;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) nudge_y -= nudge_step;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) nudge_y += nudge_step;
    if (nudge_x != 0.0f || nudge_y != 0.0f) {
        const View before = session.view();
        if (glayout::graph_canvas_nudge(session.view().layout, compiled.layout, geometry,
                                        state.canvas, nudge_x, nudge_y)) {
            session.commit_snapshot(before);
            if (hooks.rebuild) hooks.rebuild();
        }
    }

    const auto [mouse_x, mouse_y] = logical_point(io.MousePos, map);
    if (!io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (state.focus_authoring) {
            const NodeIndex hit = hit_focus_node(compiled, geometry, mouse_x, mouse_y);
            if (hit != invalid_node) {
                const std::string id = compiled.nodes[hit].source.layout_id;
                if (state.edge_source.empty())
                    state.edge_source = id;
                else
                    state.edge_target = id;
                session.select(id);
            }
        } else {
            state.transaction = session.view();
            const glayout::CanvasResult result =
                glayout::graph_canvas_press(session.view().layout, compiled.layout, geometry,
                                            state.canvas, mouse_x, mouse_y, io.KeyShift);
            if (!result.transaction_started) state.transaction.reset();
            if (result.selection_changed) session.select(state.canvas.primary);
        }
    }
    if (!state.focus_authoring && state.canvas.dragging &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (glayout::graph_canvas_drag(session.view().layout, compiled.layout, geometry,
                                       state.canvas, mouse_x, mouse_y)
                .changed &&
            hooks.rebuild)
            hooks.rebuild();
    }
    if (!state.focus_authoring && state.canvas.dragging &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        const glayout::CanvasResult result = glayout::graph_canvas_release(
            session.view().layout, compiled.layout, geometry, state.canvas, mouse_x, mouse_y);
        if (result.changed && state.transaction)
            session.commit_snapshot(std::move(*state.transaction));
        state.transaction.reset();
        if (result.changed && hooks.rebuild) hooks.rebuild();
    }
}

} // namespace gview
