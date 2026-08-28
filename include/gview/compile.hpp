#pragma once

#include "gview/view.hpp"

#include <unordered_map>

namespace gview {

struct CompiledFocusEdge {
    NodeIndex from = invalid_node;
    NavAction action = NavAction::Down;
    NodeIndex to = invalid_node;
};

struct CompiledFocusGroup {
    std::string id;
    NodeIndex entry = invalid_node;
    NodeIndex owner = invalid_node;
    bool contain = false;
    bool remember = true;
};

struct CompiledNode {
    NodeSpec source;
    glayout::NodeIndex layout_index = glayout::invalid_node_index;
};

struct CompiledView {
    std::string id;
    std::string label;
    glayout::CompiledGraph layout;
    std::vector<CompiledNode> nodes;
    std::vector<CompiledFocusGroup> focus_groups;
    std::vector<CompiledFocusEdge> focus_edges;
    std::vector<NodeIndex> layout_to_node;
    std::unordered_map<std::string, NodeIndex> indices;
};

struct CompileResult {
    bool ok = false;
    CompiledView view;
    std::vector<glayout::Diagnostic> diagnostics;
};

CompileResult compile_view(const View& view);

} // namespace gview
