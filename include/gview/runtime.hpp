#pragma once

#include "gview/paint.hpp"

#include <functional>
#include <unordered_map>

namespace gview {

struct PointerInput {
    float x = 0.0f;
    float y = 0.0f;
    bool moved = false;
    bool pressed = false;
    bool released = false;
    float scroll_y = 0.0f;
};

struct InputFrame {
    PointerInput pointer;
    std::vector<NavAction> navigation;
    std::string text;
};

struct Host {
    std::function<Value(std::string_view)> read;
    std::function<void(std::string_view, const Value&)> write;
    std::function<void(std::string_view, NodeIndex)> action;
    std::function<bool(std::string_view)> condition;
    glayout::MeasureFunction measure = nullptr;
    void* measure_user_data = nullptr;
    std::uint64_t revision = 0;
};

struct NodeState {
    bool hovered = false;
    bool pressed = false;
    bool open = false;
    float scroll = 0.0f;
    std::size_t pending_option = 0;
    bool editing = false;
    std::string edit_text;
};

struct RuntimeStats {
    std::uint64_t frames = 0;
    std::uint64_t paint_builds = 0;
    std::uint64_t layout_builds = 0;
    std::uint64_t actions = 0;
};

class Runtime {
  public:
    Runtime() = default;
    explicit Runtime(CompiledView view);

    void reset(CompiledView view);
    void frame(const glayout::ResolveInput& resolution, const InputFrame& input, Host& host);
    void invalidate_paint();

    NodeIndex focus() const;
    bool set_focus(std::string_view id);
    const std::vector<PaintCommand>& paint() const;
    const std::vector<NodeState>& state() const;
    const RuntimeStats& stats() const;
    const CompiledView& view() const;

  private:
    CompiledView view_;
    glayout::GraphRuntime layout_;
    std::vector<NodeState> state_;
    std::vector<PaintCommand> paint_;
    std::unordered_map<std::string, std::string> remembered_focus_;
    NodeIndex focus_ = invalid_node;
    bool paint_dirty_ = true;
    std::uint64_t host_revision_ = 0;
    RuntimeStats stats_;
};

} // namespace gview
