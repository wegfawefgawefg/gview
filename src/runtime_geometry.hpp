#pragma once

#include "gview/runtime.hpp"

namespace gview::detail {

// Applies every retained ancestor scroll offset to resolved layout geometry.
inline glayout::Rect displayed_rect(const CompiledView& view,
                                    const std::vector<NodeState>& state,
                                    glayout::Rect rect,
                                    glayout::NodeIndex layout_index) {
    glayout::NodeIndex parent = view.layout.nodes[layout_index].parent;
    while (parent != glayout::invalid_node_index) {
        const NodeIndex presentation = view.layout_to_node[parent];
        if (presentation != invalid_node &&
            view.nodes[presentation].source.control == ControlKind::ScrollArea)
            rect.y -= state[presentation].scroll;
        parent = view.layout.nodes[parent].parent;
    }
    return rect;
}

} // namespace gview::detail
