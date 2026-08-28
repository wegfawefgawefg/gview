#include "gview/runtime.hpp"

namespace gview {
namespace {

std::size_t string_bytes(const std::string& value) { return value.capacity() + 1; }

std::size_t node_bytes(const NodeSpec& node) {
    std::size_t bytes = sizeof(NodeSpec) + string_bytes(node.layout_id) + string_bytes(node.text) +
                        string_bytes(node.asset) + string_bytes(node.binding) +
                        string_bytes(node.action) + string_bytes(node.focus_group) +
                        string_bytes(node.style_class) + string_bytes(node.condition);
    bytes += node.options.capacity() * sizeof(SelectOption);
    for (const SelectOption& option : node.options)
        bytes += string_bytes(option.id) + string_bytes(option.label) +
                 (std::get_if<std::string>(&option.value)
                      ? string_bytes(std::get<std::string>(option.value))
                      : 0);
    return bytes;
}

} // namespace

// Estimates heap storage owned by one activated view, excluding backend
// textures and host data.
std::size_t Runtime::owned_bytes() const {
    std::size_t bytes = view_.nodes.capacity() * sizeof(CompiledNode) +
                        view_.focus_groups.capacity() * sizeof(CompiledFocusGroup) +
                        view_.focus_edges.capacity() * sizeof(CompiledFocusEdge) +
                        view_.focus_group_edges.capacity() * sizeof(CompiledFocusGroupEdge) +
                        view_.layout_to_node.capacity() * sizeof(NodeIndex) +
                        state_.capacity() * sizeof(NodeState) +
                        paint_.capacity() * sizeof(PaintCommand) +
                        layout_.nodes().capacity() * sizeof(glayout::ResolvedNode);
    for (const CompiledNode& node : view_.nodes) {
        bytes += node_bytes(node.source);
        bytes += node.skin.parts.capacity() * sizeof(PartPresentation);
        for (const PartPresentation& part : node.skin.parts)
            bytes += string_bytes(part.asset);
    }
    for (const NodeState& state : state_)
        bytes += string_bytes(state.edit_text);
    for (const PaintCommand& command : paint_)
        bytes += string_bytes(command.text) + string_bytes(command.asset);
    for (const auto& [id, index] : view_.indices) {
        (void)index;
        bytes += sizeof(decltype(view_.indices)::value_type) + string_bytes(id);
    }
    for (const auto& [group, node] : remembered_focus_)
        bytes += sizeof(decltype(remembered_focus_)::value_type) + string_bytes(group) +
                 string_bytes(node);
    for (const CompiledFocusGroupEdge& edge : view_.focus_group_edges)
        bytes += string_bytes(edge.from) + string_bytes(edge.to);
    return bytes;
}

} // namespace gview
