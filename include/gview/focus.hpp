#pragma once

#include "gview/compile.hpp"

#include <functional>

namespace gview {

using NodeAvailable = std::function<bool(NodeIndex)>;

NodeIndex first_focus(const CompiledView& view, const NodeAvailable& available);
NodeIndex next_focus(const CompiledView& view, const std::vector<glayout::ResolvedNode>& geometry,
                     NodeIndex current, NavAction action, const NodeAvailable& available);
const CompiledFocusGroup* focus_group_for(const CompiledView& view, NodeIndex node);

struct FocusIssue {
    NodeIndex node = invalid_node;
    std::string message;
};

std::vector<FocusIssue> analyze_focus_graph(const CompiledView& view,
                                            const std::vector<glayout::ResolvedNode>& geometry);

} // namespace gview
