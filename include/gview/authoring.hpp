#pragma once

#include "gview/io.hpp"

#include <filesystem>
#include <functional>

namespace gview {

struct PreviewConfig {
    int width = 1280;
    int height = 720;
    float dpi_scale = 1.0f;
    glayout::FormFactor form_factor = glayout::FormFactor::Desktop;
    glayout::Insets safe_area;
    std::string state = "default";
};

class AuthoringSession {
  public:
    void open(View view, std::filesystem::path source = {});

    const View& view() const;
    View& view();
    const std::filesystem::path& source() const;
    const std::string& selection() const;
    void select(std::string id);
    bool dirty() const;
    bool can_undo() const;
    bool can_redo() const;

    bool edit(const std::function<bool(View&)>& mutation);
    void commit_snapshot(View before);
    bool add(std::string_view parent, glayout::GraphNode layout, NodeSpec presentation);
    bool remove(std::string_view id);
    bool duplicate(std::string_view id, std::string new_id);
    bool reparent(std::string_view id, std::string_view parent);
    bool connect(std::string from, NavAction action, std::string to);
    bool disconnect(std::string_view from, NavAction action, std::string_view to);
    bool undo();
    bool redo();
    bool save();
    bool save_as(const std::filesystem::path& path);
    bool reload();
    std::string make_id(std::string_view prefix = "node");

  private:
    View view_;
    std::filesystem::path source_;
    std::string selection_;
    std::vector<View> undo_;
    std::vector<View> redo_;
    std::uint64_t next_id_ = 1;
    bool dirty_ = false;
};

} // namespace gview
