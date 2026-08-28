#include "gview/compile.hpp"

#include <unordered_set>

namespace gview {
namespace {

void error(CompileResult& result, std::string message) {
    result.diagnostics.push_back(
        glayout::Diagnostic{glayout::DiagnosticSeverity::Error, std::move(message), 1, 1});
}

NodeIndex index_for(const CompiledView& view, std::string_view layout_id) {
    const auto found = view.indices.find(std::string(layout_id));
    return found == view.indices.end() ? invalid_node : found->second;
}

} // namespace

// Joins presentation and focus source data to one validated dense layout graph.
CompileResult compile_view(const View& source) {
    CompileResult result;
    result.view.id = source.id;
    result.view.label = source.label;
    if (source.id.empty())
        error(result, "view has an empty id");

    glayout::GraphCompileResult layout = glayout::compile_graph(source.layout);
    result.diagnostics.insert(result.diagnostics.end(), layout.diagnostics.begin(),
                              layout.diagnostics.end());
    result.view.layout = std::move(layout.graph);
    result.view.layout_to_node.assign(result.view.layout.nodes.size(), invalid_node);

    for (const NodeSpec& node : source.nodes) {
        const auto layout_index = result.view.layout.indices.find(node.layout_id);
        if (layout_index == result.view.layout.indices.end()) {
            error(result, "presentation node references missing layout node '" + node.layout_id + "'");
            continue;
        }
        if (result.view.indices.contains(node.layout_id)) {
            error(result, "duplicate presentation node '" + node.layout_id + "'");
            continue;
        }
        const NodeIndex index = static_cast<NodeIndex>(result.view.nodes.size());
        result.view.indices.emplace(node.layout_id, index);
        result.view.nodes.push_back(CompiledNode{node, layout_index->second});
        result.view.layout_to_node[layout_index->second] = index;
    }

    std::unordered_set<std::string> group_ids;
    for (const FocusGroup& group : source.focus_groups) {
        if (group.id.empty() || !group_ids.insert(group.id).second) {
            error(result, "duplicate or empty focus group id '" + group.id + "'");
            continue;
        }
        CompiledFocusGroup compiled;
        compiled.id = group.id;
        compiled.entry = index_for(result.view, group.entry);
        compiled.owner = index_for(result.view, group.owner);
        compiled.contain = group.contain;
        compiled.remember = group.remember;
        if (!group.entry.empty() && compiled.entry == invalid_node)
            error(result, "focus group '" + group.id + "' has a missing entry");
        if (!group.owner.empty() && compiled.owner == invalid_node)
            error(result, "focus group '" + group.id + "' has a missing owner");
        result.view.focus_groups.push_back(std::move(compiled));
    }

    for (const FocusEdge& edge : source.focus_edges) {
        const NodeIndex from = index_for(result.view, edge.from);
        const NodeIndex to = index_for(result.view, edge.to);
        if (from == invalid_node || to == invalid_node) {
            error(result, "focus edge references a missing node");
            continue;
        }
        result.view.focus_edges.push_back(CompiledFocusEdge{from, edge.action, to});
    }

    result.ok = result.diagnostics.empty();
    return result;
}

} // namespace gview
