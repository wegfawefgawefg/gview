#pragma once

#include "gview/runtime.hpp"

namespace gview::detail {

void compose(const CompiledView& view, const glayout::GraphRuntime& layout,
             const std::vector<NodeState>& state, NodeIndex focus, const Host& host,
             std::vector<PaintCommand>& output);

} // namespace gview::detail
