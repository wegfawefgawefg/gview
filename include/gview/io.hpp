#pragma once

#include "gview/view.hpp"

#include <filesystem>

namespace gview {

struct ParseResult {
    bool ok = false;
    std::vector<View> views;
    std::vector<glayout::Diagnostic> diagnostics;
};

ParseResult parse_views(std::string_view text);
std::string write_views(const std::vector<View>& views);
ParseResult load_view_file(const std::filesystem::path& path);
bool save_view_file(const std::filesystem::path& path, const std::vector<View>& views);

} // namespace gview
