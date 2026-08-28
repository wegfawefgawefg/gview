#include "gview/runtime.hpp"

#include "gview/focus.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace gview {
namespace {

bool contains(glayout::Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y && x <= rect.x + rect.w && y <= rect.y + rect.h;
}

glayout::Rect shifted_rect(const CompiledView& view, const std::vector<NodeState>& state,
                           glayout::Rect rect, glayout::NodeIndex layout_index) {
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

std::string value_text(const Value& value, std::string fallback) {
    std::string resolved;
    if (const std::string* text = std::get_if<std::string>(&value)) resolved = *text;
    else if (const bool* boolean = std::get_if<bool>(&value)) resolved = *boolean ? "On" : "Off";
    if (const std::int64_t* integer = std::get_if<std::int64_t>(&value))
        resolved = std::to_string(*integer);
    else if (const double* number = std::get_if<double>(&value)) {
        resolved = std::to_string(*number);
        while (resolved.size() > 1 && resolved.back() == '0') resolved.pop_back();
        if (!resolved.empty() && resolved.back() == '.') resolved.pop_back();
    }
    if (resolved.empty()) return fallback;
    return fallback.empty() ? resolved : fallback + "\n" + resolved;
}

bool visible(const CompiledNode& node, const glayout::GraphRuntime& layout, const Host& host) {
    const glayout::ResolvedNode& geometry = layout.nodes()[node.layout_index];
    if (!geometry.visible || !node.source.enabled)
        return false;
    return node.source.condition.empty() || !host.condition || host.condition(node.source.condition);
}

void emit_action(RuntimeStats& stats, const CompiledNode& node, NodeIndex index, Host& host) {
    if (!node.source.action.empty() && host.action) {
        host.action(node.source.action, index);
        ++stats.actions;
    }
}

void activate(RuntimeStats& stats, const CompiledNode& node, NodeState& state, NodeIndex index,
              Host& host) {
    const NodeSpec& source = node.source;
    if (source.control == ControlKind::Toggle && !source.binding.empty() && host.write) {
        const Value current = host.read ? host.read(source.binding) : Value{false};
        host.write(source.binding, !std::get_if<bool>(&current) || !std::get<bool>(current));
    } else if (source.control == ControlKind::Select) {
        state.open = true;
        state.pending_option = 0;
        if (host.read) {
            const Value current = host.read(source.binding);
            for (std::size_t option = 0; option < source.options.size(); ++option) {
                if (source.options[option].value == current) state.pending_option = option;
            }
        }
    } else if (source.control == ControlKind::TextInput) {
        state.editing = true;
        state.edit_text.clear();
        if (host.read) {
            const Value current = host.read(source.binding);
            if (const std::string* text = std::get_if<std::string>(&current))
                state.edit_text = *text;
        }
    } else {
        emit_action(stats, node, index, host);
    }
}

void ensure_focus_visible(const CompiledView& view, const glayout::GraphRuntime& layout,
                          std::vector<NodeState>& state, NodeIndex focus) {
    if (focus == invalid_node) return;
    const CompiledNode& focused = view.nodes[focus];
    const glayout::Rect target = layout.nodes()[focused.layout_index].border;
    glayout::NodeIndex parent = view.layout.nodes[focused.layout_index].parent;
    while (parent != glayout::invalid_node_index) {
        const NodeIndex presentation = view.layout_to_node[parent];
        if (presentation != invalid_node &&
            view.nodes[presentation].source.control == ControlKind::ScrollArea) {
            const glayout::Rect viewport = layout.nodes()[parent].content;
            const float maximum = std::max(
                0.0f, layout.nodes()[parent].content_extent.height - viewport.h);
            const float displayed_top = target.y - state[presentation].scroll;
            const float displayed_bottom = displayed_top + target.h;
            if (displayed_top < viewport.y)
                state[presentation].scroll =
                    std::clamp(state[presentation].scroll - (viewport.y - displayed_top), 0.0f,
                               maximum);
            else if (displayed_bottom > viewport.y + viewport.h)
                state[presentation].scroll =
                    std::clamp(state[presentation].scroll +
                                   (displayed_bottom - viewport.y - viewport.h),
                               0.0f, maximum);
        }
        parent = view.layout.nodes[parent].parent;
    }
}

void adjust_slider(const NodeSpec& source, int direction, Host& host) {
    if (source.binding.empty() || !host.write)
        return;
    const double current = host.read ? numeric_value(host.read(source.binding), source.minimum)
                                     : source.minimum;
    const double value = std::clamp(current + source.step * static_cast<double>(direction),
                                    source.minimum, source.maximum);
    host.write(source.binding, value);
}

void set_slider_from_pointer(const NodeSpec& source, glayout::Rect rect, float pointer_x,
                             Host& host) {
    if (source.binding.empty() || !host.write || rect.w <= 0.0f)
        return;
    const double ratio = std::clamp(static_cast<double>((pointer_x - rect.x) / rect.w), 0.0, 1.0);
    const double raw = source.minimum + (source.maximum - source.minimum) * ratio;
    const double steps = source.step > 0.0 ? std::round((raw - source.minimum) / source.step) : 0.0;
    host.write(source.binding, source.step > 0.0 ? source.minimum + steps * source.step : raw);
}

void commit_select(RuntimeStats& stats, const CompiledNode& node, NodeState& state,
                   NodeIndex index, Host& host) {
    if (host.write && !node.source.binding.empty() && !node.source.options.empty())
        host.write(node.source.binding, node.source.options[state.pending_option].value);
    state.open = false;
    emit_action(stats, node, index, host);
}

} // namespace

// Initializes retained interaction state around a compiled immutable view.
Runtime::Runtime(CompiledView view) {
    reset(std::move(view));
}

void Runtime::reset(CompiledView view) {
    view_ = std::move(view);
    layout_.reset(view_.layout);
    state_.assign(view_.nodes.size(), {});
    paint_.clear();
    focus_ = invalid_node;
    paint_dirty_ = true;
    host_revision_ = 0;
    stats_ = {};
}

// Applies pointer and semantic navigation without pretending a controller is a mouse.
void Runtime::frame(const glayout::ResolveInput& resolution, const InputFrame& input, Host& host) {
    ++stats_.frames;
    if (host.revision != host_revision_) {
        host_revision_ = host.revision;
        paint_dirty_ = true;
    }
    glayout::ResolveInput measured = resolution;
    measured.measure = host.measure;
    measured.measure_user_data = host.measure_user_data;
    if (layout_.resolve(measured)) {
        ++stats_.layout_builds;
        paint_dirty_ = true;
    }
    const auto available = [&](NodeIndex index) { return visible(view_.nodes[index], layout_, host); };
    if (focus_ == invalid_node || !available(focus_))
        focus_ = first_focus(view_, available);

    NodeIndex pointer_target = invalid_node;
    bool pointer_select_option = false;
    if (input.pointer.moved || input.pointer.pressed || input.pointer.released) {
        for (NodeState& state : state_) state.hovered = false;
        for (NodeIndex index = 0; index < view_.nodes.size(); ++index) {
            const CompiledNode& node = view_.nodes[index];
            if (!state_[index].open || node.source.control != ControlKind::Select ||
                node.source.options.empty())
                continue;
            glayout::Rect option = shifted_rect(view_, state_,
                                                layout_.nodes()[node.layout_index].border,
                                                node.layout_index);
            option.y += option.h;
            option.h *= static_cast<float>(node.source.options.size());
            if (contains(option, input.pointer.x, input.pointer.y)) {
                const float row_height = layout_.nodes()[node.layout_index].border.h;
                const std::size_t selected = static_cast<std::size_t>(
                    std::clamp(static_cast<int>((input.pointer.y - option.y) / row_height), 0,
                               static_cast<int>(node.source.options.size()) - 1));
                state_[index].pending_option = selected;
                state_[index].hovered = true;
                pointer_target = index;
                pointer_select_option = true;
                break;
            }
        }
        for (std::size_t reverse = view_.nodes.size();
             pointer_target == invalid_node && reverse > 0; --reverse) {
            const NodeIndex index = static_cast<NodeIndex>(reverse - 1);
            const CompiledNode& node = view_.nodes[index];
            const glayout::ResolvedNode& geometry = layout_.nodes()[node.layout_index];
            const glayout::Rect border = shifted_rect(view_, state_, geometry.border, node.layout_index);
            if (node.source.focusable && available(index) &&
                contains(border, input.pointer.x, input.pointer.y) &&
                contains(geometry.clip, input.pointer.x, input.pointer.y)) {
                pointer_target = index;
                state_[index].hovered = true;
                break;
            }
        }
        paint_dirty_ = true;
    }
    if (input.pointer.pressed && pointer_target != invalid_node) {
        focus_ = pointer_target;
        state_[pointer_target].pressed = true;
        paint_dirty_ = true;
    }
    if ((input.pointer.pressed || input.pointer.moved) && focus_ != invalid_node &&
        state_[focus_].pressed && view_.nodes[focus_].source.control == ControlKind::Slider) {
        const CompiledNode& slider = view_.nodes[focus_];
        const glayout::Rect rect = shifted_rect(view_, state_,
                                                layout_.nodes()[slider.layout_index].content,
                                                slider.layout_index);
        set_slider_from_pointer(slider.source, rect, input.pointer.x, host);
        paint_dirty_ = true;
    }
    if (input.pointer.released) {
        for (NodeIndex index = 0; index < state_.size(); ++index) {
            if (state_[index].pressed && index == pointer_target) {
                if (pointer_select_option)
                    commit_select(stats_, view_.nodes[index], state_[index], index, host);
                else
                    activate(stats_, view_.nodes[index], state_[index], index, host);
            }
            state_[index].pressed = false;
        }
        paint_dirty_ = true;
    }
    if (input.pointer.scroll_y != 0.0f) {
        for (NodeIndex index = 0; index < view_.nodes.size(); ++index) {
            const CompiledNode& node = view_.nodes[index];
            const glayout::ResolvedNode& geometry = layout_.nodes()[node.layout_index];
            if (node.source.control != ControlKind::ScrollArea ||
                !contains(geometry.border, input.pointer.x, input.pointer.y))
                continue;
            const float maximum = std::max(0.0f, geometry.content_extent.height - geometry.content.h);
            state_[index].scroll =
                std::clamp(state_[index].scroll - input.pointer.scroll_y * 40.0f, 0.0f, maximum);
            paint_dirty_ = true;
            break;
        }
    }

    if (focus_ != invalid_node && state_[focus_].editing &&
        view_.nodes[focus_].source.control == ControlKind::TextInput && !input.text.empty()) {
        for (char character : input.text) {
            if (character == '\b') {
                if (!state_[focus_].edit_text.empty()) state_[focus_].edit_text.pop_back();
            } else {
                state_[focus_].edit_text.push_back(character);
            }
        }
        paint_dirty_ = true;
    }

    for (NavAction action : input.navigation) {
        if (focus_ == invalid_node)
            break;
        CompiledNode& node = view_.nodes[focus_];
        NodeState& state = state_[focus_];
        if (state.editing && node.source.control == ControlKind::TextInput) {
            if (action == NavAction::Confirm) {
                if (host.write && !node.source.binding.empty())
                    host.write(node.source.binding, state.edit_text);
                state.editing = false;
                emit_action(stats_, node, focus_, host);
            } else if (action == NavAction::Back) {
                state.editing = false;
                state.edit_text.clear();
            }
            paint_dirty_ = true;
            continue;
        }
        if (state.open && node.source.control == ControlKind::Select) {
            if ((action == NavAction::Up || action == NavAction::Down) && !node.source.options.empty()) {
                const int step = action == NavAction::Up ? -1 : 1;
                const int count = static_cast<int>(node.source.options.size());
                int option = static_cast<int>(state.pending_option) + step;
                if (option < 0) option = count - 1;
                if (option >= count) option = 0;
                state.pending_option = static_cast<std::size_t>(option);
            } else if (action == NavAction::Confirm) {
                commit_select(stats_, node, state, focus_, host);
            } else if (action == NavAction::Back) {
                state.open = false;
            }
            paint_dirty_ = true;
            continue;
        }
        if (node.source.control == ControlKind::Slider &&
            (action == NavAction::Left || action == NavAction::Right)) {
            adjust_slider(node.source, action == NavAction::Left ? -1 : 1, host);
            paint_dirty_ = true;
            continue;
        }
        if (action == NavAction::Confirm) {
            const NodeIndex explicit_target =
                next_focus(view_, layout_.nodes(), focus_, action, available);
            if (explicit_target != focus_) {
                focus_ = explicit_target;
                ensure_focus_visible(view_, layout_, state_, focus_);
                paint_dirty_ = true;
                continue;
            }
            const auto owned_group = std::find_if(
                view_.focus_groups.begin(), view_.focus_groups.end(),
                [&](const CompiledFocusGroup& group) { return group.owner == focus_; });
            if (owned_group != view_.focus_groups.end()) {
                NodeIndex destination = owned_group->entry;
                const auto remembered = remembered_focus_.find(owned_group->id);
                if (remembered != remembered_focus_.end()) {
                    const auto found = view_.indices.find(remembered->second);
                    if (found != view_.indices.end() && available(found->second))
                        destination = found->second;
                }
                if (destination != invalid_node && available(destination)) {
                    focus_ = destination;
                    paint_dirty_ = true;
                    continue;
                }
            }
            activate(stats_, node, state, focus_, host);
            paint_dirty_ = true;
            continue;
        }
        if (action == NavAction::Back) {
            const NodeIndex explicit_target =
                next_focus(view_, layout_.nodes(), focus_, action, available);
            if (explicit_target != focus_) {
                focus_ = explicit_target;
                ensure_focus_visible(view_, layout_, state_, focus_);
                paint_dirty_ = true;
                continue;
            }
            const CompiledFocusGroup* group = focus_group_for(view_, focus_);
            if (group && group->owner != invalid_node) {
                if (group->remember)
                    remembered_focus_[group->id] = view_.nodes[focus_].source.layout_id;
                focus_ = group->owner;
            } else if (host.action) {
                host.action("back", focus_);
                ++stats_.actions;
            }
            paint_dirty_ = true;
            continue;
        }
        const NodeIndex previous = focus_;
        focus_ = next_focus(view_, layout_.nodes(), focus_, action, available);
        for (const CompiledFocusGroup& group : view_.focus_groups) {
            if (group.owner != previous || group.entry != focus_) continue;
            const auto remembered = remembered_focus_.find(group.id);
            if (remembered == remembered_focus_.end()) continue;
            const auto found = view_.indices.find(remembered->second);
            if (found != view_.indices.end() && available(found->second)) focus_ = found->second;
            break;
        }
        if (focus_ != previous) {
            ensure_focus_visible(view_, layout_, state_, focus_);
            const CompiledNode& focused = view_.nodes[focus_];
            if (focused.source.activation == ActivationPolicy::OnFocus)
                activate(stats_, focused, state_[focus_], focus_, host);
            paint_dirty_ = true;
        }
    }

    if (paint_dirty_) {
        paint_.clear();
        paint_.reserve(view_.nodes.size() * 2);
        for (NodeIndex index = 0; index < view_.nodes.size(); ++index) {
            const CompiledNode& node = view_.nodes[index];
            if (!available(index)) continue;
            const glayout::ResolvedNode& geometry = layout_.nodes()[node.layout_index];
            const glayout::Rect border = shifted_rect(view_, state_, geometry.border, node.layout_index);
            const glayout::Rect content = shifted_rect(view_, state_, geometry.content, node.layout_index);
            if (!glayout::intersects(border, geometry.clip)) continue;
            const BoxStyle& style = resolve_style(node.source, state_[index], focus_ == index);
            paint_.push_back(PaintCommand{PaintKind::Box, node.source.stratum, index, border,
                                          geometry.clip, style, {}, {}, {}, 0.0});
            const Value bound = host.read && !node.source.binding.empty()
                                    ? host.read(node.source.binding) : Value{};
            if (node.source.control == ControlKind::Slider) {
                glayout::Rect track = content;
                track.y = content.y + content.h * 0.65f;
                track.h = std::max(3.0f, content.h * 0.12f);
                const double range = node.source.maximum - node.source.minimum;
                const double ratio = range > 0.0
                                         ? (numeric_value(bound, node.source.minimum) -
                                            node.source.minimum) / range
                                         : 0.0;
                paint_.push_back(PaintCommand{PaintKind::Progress, node.source.stratum, index,
                                              track, geometry.clip, style, {}, {}, {}, ratio});
            } else if (node.source.control == ControlKind::Toggle) {
                glayout::Rect indicator{content.x + content.w - 34.0f, content.y + 8.0f,
                                        26.0f, std::max(16.0f, content.h - 16.0f)};
                BoxStyle toggle_style = style;
                const bool enabled = std::get_if<bool>(&bound) && std::get<bool>(bound);
                toggle_style.fill = enabled ? Color{146, 239, 117, 255} : Color{35, 52, 57, 255};
                paint_.push_back(PaintCommand{PaintKind::Box, node.source.stratum, index,
                                              indicator, geometry.clip, toggle_style, {}, {}, {}, 0.0});
            }
            PaintKind kind = PaintKind::Text;
            if (node.source.content == ContentKind::Image) kind = PaintKind::Image;
            else if (node.source.content == ContentKind::Sprite) kind = PaintKind::Sprite;
            else if (node.source.content == ContentKind::Progress) kind = PaintKind::Progress;
            else if (node.source.content == ContentKind::CustomSurface) kind = PaintKind::CustomSurface;
            if (node.source.content != ContentKind::None) {
                std::string rendered_text = value_text(bound, node.source.text);
                if (node.source.control == ControlKind::TextInput && state_[index].editing)
                    rendered_text = node.source.text.empty()
                                        ? state_[index].edit_text + "|"
                                        : node.source.text + "\n" + state_[index].edit_text + "|";
                paint_.push_back(PaintCommand{kind, node.source.stratum, index, content,
                                              geometry.clip, style, node.source.text_style,
                                              std::move(rendered_text), node.source.asset,
                                              numeric_value(bound)});
            }
            if (state_[index].open && node.source.control == ControlKind::Select) {
                for (std::size_t option = 0; option < node.source.options.size(); ++option) {
                    glayout::Rect option_rect = border;
                    option_rect.y += border.h * static_cast<float>(option + 1);
                    const BoxStyle& option_style = option == state_[index].pending_option
                                                       ? node.source.style.focused
                                                       : node.source.style.normal;
                    paint_.push_back(PaintCommand{PaintKind::Box, PaintStratum::Overlay, index,
                                                  option_rect, geometry.clip, option_style, {}, {}, {}, 0.0});
                    paint_.push_back(PaintCommand{PaintKind::Text, PaintStratum::Overlay, index,
                                                  option_rect, geometry.clip, option_style,
                                                  node.source.text_style,
                                                  node.source.options[option].label, {}, 0.0});
                }
            }
        }
        std::stable_sort(paint_.begin(), paint_.end(), [](const PaintCommand& left,
                                                          const PaintCommand& right) {
            return left.stratum < right.stratum;
        });
        ++stats_.paint_builds;
        paint_dirty_ = false;
    }
}

void Runtime::invalidate_paint() { paint_dirty_ = true; }
NodeIndex Runtime::focus() const { return focus_; }

bool Runtime::set_focus(std::string_view id) {
    const auto found = view_.indices.find(std::string(id));
    if (found == view_.indices.end() || !view_.nodes[found->second].source.focusable)
        return false;
    focus_ = found->second;
    if (!layout_.nodes().empty()) ensure_focus_visible(view_, layout_, state_, focus_);
    paint_dirty_ = true;
    return true;
}

const std::vector<PaintCommand>& Runtime::paint() const { return paint_; }
const std::vector<NodeState>& Runtime::state() const { return state_; }
const RuntimeStats& Runtime::stats() const { return stats_; }
const CompiledView& Runtime::view() const { return view_; }
const std::vector<glayout::ResolvedNode>& Runtime::geometry() const { return layout_.nodes(); }

} // namespace gview
