#pragma once

#include "gview/view.hpp"

#include <gsexp/sexp.hpp>

#include <sstream>

namespace gview::detail {

void parse_themes(gsexp::Node source, View& view);
void write_themes(std::ostringstream& output, const View& view);

} // namespace gview::detail
