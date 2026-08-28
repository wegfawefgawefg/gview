#include "gview/focus.hpp"

#include <cmath>
#include <limits>
#include <queue>

namespace gview {
namespace {

bool directional(NavAction action) {
    return action == NavAction::Up || action == NavAction::Down || action == NavAction::Left ||
           action == NavAction::Right;
}

bool same_group(const CompiledView& view, NodeIndex left, NodeIndex right) {
    return view.nodes[left].source.focus_group == view.nodes[right].source.focus_group;
}

float direction_score(glayout::Rect source, glayout::Rect target, NavAction action) {
    const float source_x = source.x + source.w * 0.5f;
    const float source_y = source.y + source.h * 0.5f;
    const float dx = target.x + target.w * 0.5f - source_x;
    const float dy = target.y + target.h * 0.5f - source_y;
    float primary = 0.0f;
    float secondary = 0.0f;
    if (action == NavAction::Left) {
        if (dx >= 0.0f) return std::numeric_limits<float>::max();
        primary = -dx;
        secondary = std::fabs(dy);
    } else if (action == NavAction::Right) {
        if (dx <= 0.0f) return std::numeric_limits<float>::max();
        primary = dx;
        secondary = std::fabs(dy);
    } else if (action == NavAction::Up) {
        if (dy >= 0.0f) return std::numeric_limits<float>::max();
        primary = -dy;
        secondary = std::fabs(dx);
    } else {
        if (dy <= 0.0f) return std::numeric_limits<float>::max();
        primary = dy;
        secondary = std::fabs(dx);
    }
    return primary * 4.0f + secondary;
}

} // namespace

// Finds a deterministic initial target without assuming a pointer cursor.
NodeIndex first_focus(const CompiledView& view, const NodeAvailable& available) {
    for (NodeIndex index = 0; index < view.nodes.size(); ++index) {
        if (view.nodes[index].source.focusable && available(index)) return index;
    }
    return invalid_node;
}

// Resolves explicit overrides first and local geometric relationships second.
NodeIndex next_focus(const CompiledView& view, const std::vector<glayout::ResolvedNode>& geometry,
                     NodeIndex current, NavAction action, const NodeAvailable& available) {
    if (current == invalid_node || current >= view.nodes.size())
        return first_focus(view, available);
    for (const CompiledFocusEdge& edge : view.focus_edges) {
        if (edge.from == current && edge.action == action && available(edge.to)) return edge.to;
    }
    if (action == NavAction::TabPrevious || action == NavAction::TabNext) {
        const int step = action == NavAction::TabPrevious ? -1 : 1;
        int candidate = static_cast<int>(current);
        for (std::size_t count = 0; count < view.nodes.size(); ++count) {
            candidate += step;
            if (candidate < 0) candidate = static_cast<int>(view.nodes.size()) - 1;
            if (candidate >= static_cast<int>(view.nodes.size())) candidate = 0;
            const NodeIndex index = static_cast<NodeIndex>(candidate);
            if (same_group(view, current, index) && view.nodes[index].source.focusable &&
                available(index))
                return index;
        }
        return current;
    }
    if (!directional(action)) return current;

    const glayout::Rect source = geometry[view.nodes[current].layout_index].border;
    NodeIndex best = current;
    float best_score = std::numeric_limits<float>::max();
    for (NodeIndex candidate = 0; candidate < view.nodes.size(); ++candidate) {
        if (candidate == current || !same_group(view, current, candidate) ||
            !view.nodes[candidate].source.focusable || !available(candidate))
            continue;
        const glayout::Rect target = geometry[view.nodes[candidate].layout_index].border;
        const float score = direction_score(source, target, action);
        if (score < best_score) {
            best = candidate;
            best_score = score;
        }
    }
    return best;
}

// Looks up the authored local focus scope for back and memory behavior.
const CompiledFocusGroup* focus_group_for(const CompiledView& view, NodeIndex node) {
    if (node == invalid_node || node >= view.nodes.size()) return nullptr;
    const std::string& id = view.nodes[node].source.focus_group;
    for (const CompiledFocusGroup& group : view.focus_groups) {
        if (group.id == id) return &group;
    }
    return nullptr;
}

// Audits reachability using the same explicit, geometric, scope-entry, and Back
// rules as runtime.
std::vector<FocusIssue> analyze_focus_graph(const CompiledView& view,
                                            const std::vector<glayout::ResolvedNode>& geometry) {
    std::vector<FocusIssue> issues;
    if (geometry.size() < view.layout.nodes.size()) return issues;
    const auto available = [&](NodeIndex index) {
        return index < view.nodes.size() && view.nodes[index].source.focusable &&
               view.nodes[index].source.enabled;
    };
    const NodeIndex first = first_focus(view, available);
    if (first == invalid_node) return issues;
    std::vector<bool> visited(view.nodes.size(), false);
    std::queue<NodeIndex> pending;
    visited[first] = true;
    pending.push(first);
    constexpr NavAction actions[]{NavAction::Up,          NavAction::Down,    NavAction::Left,
                                  NavAction::Right,       NavAction::Confirm, NavAction::Back,
                                  NavAction::TabPrevious, NavAction::TabNext};
    const auto enqueue = [&](NodeIndex target) {
        if (target != invalid_node && target < visited.size() && available(target) &&
            !visited[target]) {
            visited[target] = true;
            pending.push(target);
        }
    };
    while (!pending.empty()) {
        const NodeIndex current = pending.front();
        pending.pop();
        for (NavAction action : actions)
            enqueue(next_focus(view, geometry, current, action, available));
        for (const CompiledFocusGroup& group : view.focus_groups) {
            if (group.owner == current) enqueue(group.entry);
            if (focus_group_for(view, current) == &group) enqueue(group.owner);
        }
    }
    for (NodeIndex index = 0; index < view.nodes.size(); ++index) {
        if (available(index) && !visited[index])
            issues.push_back(
                {index, "unreachable focus target: " + view.nodes[index].source.layout_id});
    }
    return issues;
}

} // namespace gview
