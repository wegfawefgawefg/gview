#include "composition.hpp"

#include "runtime_geometry.hpp"

#include "gview/widget.hpp"

#include <algorithm>

namespace gview::detail {
namespace {

bool available(const CompiledNode& node, const glayout::ResolvedNode& geometry, const Host& host) {
    if (!geometry.visible || !node.source.enabled) return false;
    return node.source.condition.empty() || !host.condition ||
           host.condition(node.source.condition);
}

std::string value_string(const Value& value) {
    if (const std::string* text = std::get_if<std::string>(&value)) return *text;
    if (const bool* boolean = std::get_if<bool>(&value)) return *boolean ? "On" : "Off";
    if (const std::int64_t* integer = std::get_if<std::int64_t>(&value))
        return std::to_string(*integer);
    if (const double* number = std::get_if<double>(&value)) {
        std::string result = std::to_string(*number);
        while (result.size() > 1 && result.back() == '0')
            result.pop_back();
        if (!result.empty() && result.back() == '.') result.pop_back();
        return result;
    }
    return {};
}

void box(std::vector<PaintCommand>& output, PaintStratum stratum, NodeIndex node,
         glayout::Rect rect, glayout::Rect clip, const BoxStyle& style) {
    output.push_back(PaintCommand{PaintKind::Box,
                                  stratum,
                                  node,
                                  rect,
                                  clip,
                                  style,
                                  {},
                                  {},
                                  {},
                                  0.0,
                                  ImageMode::Stretch,
                                  {255, 255, 255, 255},
                                  1.0f,
                                  8.0f,
                                  {},
                                  1.0f});
}

void text(std::vector<PaintCommand>& output, PaintStratum stratum, NodeIndex node,
          glayout::Rect rect, glayout::Rect clip, const BoxStyle& style, TextStyle text_style,
          std::string value) {
    output.push_back(PaintCommand{PaintKind::Text,
                                  stratum,
                                  node,
                                  rect,
                                  clip,
                                  style,
                                  std::move(text_style),
                                  std::move(value),
                                  {},
                                  0.0,
                                  ImageMode::Stretch,
                                  {255, 255, 255, 255},
                                  1.0f,
                                  8.0f,
                                  {},
                                  1.0f});
}

void image(std::vector<PaintCommand>& output, PaintStratum stratum, NodeIndex node,
           glayout::Rect rect, glayout::Rect clip, const PartPresentation& part) {
    if (part.asset.empty()) return;
    output.push_back(PaintCommand{PaintKind::Image,
                                  stratum,
                                  node,
                                  rect,
                                  clip,
                                  {},
                                  {},
                                  {},
                                  part.asset,
                                  0.0,
                                  part.image_mode,
                                  part.tint,
                                  part.opacity,
                                  part.slice,
                                  part.slice_margins,
                                  part.slice_scale});
}

const PartPresentation* part_for(const CompiledNode& node, WidgetPart part,
                                 PresentationState state) {
    return find_part(node.skin, part, state);
}

BoxStyle part_box(const CompiledNode& node, WidgetPart part, PresentationState state,
                  const BoxStyle& fallback) {
    const PartPresentation* presentation = part_for(node, part, state);
    return presentation && presentation->override_box ? presentation->box : fallback;
}

void visual_part(std::vector<PaintCommand>& output, const CompiledNode& node, WidgetPart part,
                 PresentationState state, PaintStratum stratum, NodeIndex index, glayout::Rect rect,
                 glayout::Rect clip, const BoxStyle& fallback) {
    box(output, stratum, index, rect, clip, part_box(node, part, state, fallback));
    const PartPresentation* presentation = part_for(node, part, state);
    if (presentation) image(output, stratum, index, rect, clip, *presentation);
}

void text_part(std::vector<PaintCommand>& output, const CompiledNode& node, WidgetPart part,
               PresentationState state, PaintStratum stratum, NodeIndex index, glayout::Rect rect,
               glayout::Rect clip, const BoxStyle& fallback, TextStyle text_style,
               std::string value) {
    const PartPresentation* presentation = part_for(node, part, state);
    const BoxStyle style =
        presentation && presentation->override_box ? presentation->box : fallback;
    if (presentation && presentation->override_box) box(output, stratum, index, rect, clip, style);
    if (presentation) image(output, stratum, index, rect, clip, *presentation);
    text(output, stratum, index, rect, clip, style, std::move(text_style), std::move(value));
}

BoxStyle track_style(const BoxStyle& source) {
    BoxStyle result = source;
    result.fill = {18, 38, 43, 255};
    result.border = {48, 78, 82, 255};
    result.border_width = 1.0f;
    return result;
}

BoxStyle fill_style(const BoxStyle& source) {
    BoxStyle result = source;
    result.fill = source.text;
    result.border = {0, 0, 0, 0};
    result.border_width = 0.0f;
    return result;
}

BoxStyle popup_style(const BoxStyle& source) {
    BoxStyle result = source;
    result.fill = {5, 18, 22, 255};
    result.border = source.border.a == 0 ? Color{74, 112, 116, 255} : source.border;
    result.border_width = std::max(1.0f, source.border_width);
    result.opacity = 1.0f;
    return result;
}

void compose_control(std::vector<PaintCommand>& output, const CompiledNode& node,
                     const NodeState& state, NodeIndex index, bool focused, glayout::Rect content,
                     glayout::Rect clip, const Value& bound) {
    const BoxStyle& style = resolve_style(node.source, state, focused);
    const PresentationState visual_state =
        presentation_state(node.source, state.hovered, state.pressed, state.open, focused, bound);
    const double range = node.source.maximum - node.source.minimum;
    const double ratio =
        range > 0.0 ? (numeric_value(bound, node.source.minimum) - node.source.minimum) / range
                    : 0.0;
    const WidgetGeometry geometry = resolve_widget_geometry(node.source, content, ratio);
    const std::string value = value_string(bound);

    if (node.source.control == ControlKind::Slider) {
        visual_part(output, node, WidgetPart::Track, visual_state, node.source.stratum, index,
                    geometry.track, clip, track_style(style));
        visual_part(output, node, WidgetPart::Fill, visual_state, node.source.stratum, index,
                    geometry.fill, clip, fill_style(style));
        BoxStyle thumb = fill_style(style);
        thumb.fill = focused ? Color{220, 255, 210, 255} : Color{154, 208, 187, 255};
        thumb.border = {245, 255, 250, 255};
        thumb.border_width = 1.0f;
        visual_part(output, node, WidgetPart::Thumb, visual_state, node.source.stratum, index,
                    geometry.thumb, clip, thumb);
        TextStyle value_style = node.source.text_style;
        value_style.horizontal = TextAlign::End;
        value_style.vertical = TextAlign::Center;
        text_part(output, node, WidgetPart::Label, visual_state, node.source.stratum, index,
                  geometry.label, clip, style, node.source.text_style, node.source.text);
        text_part(output, node, WidgetPart::CurrentValue, visual_state, node.source.stratum, index,
                  geometry.value, clip, style, value_style, value);
        return;
    }

    if (node.source.control == ControlKind::Toggle) {
        text_part(output, node, WidgetPart::Label, visual_state, node.source.stratum, index,
                  geometry.label, clip, style, node.source.text_style, node.source.text);
        BoxStyle indicator = track_style(style);
        indicator.fill = value == "On" ? Color{146, 239, 117, 255} : Color{35, 52, 57, 255};
        visual_part(output, node, WidgetPart::Indicator, visual_state, node.source.stratum, index,
                    geometry.indicator, clip, indicator);
        return;
    }

    if (node.source.control == ControlKind::Select) {
        text_part(output, node, WidgetPart::Label, visual_state, node.source.stratum, index,
                  geometry.label, clip, style, node.source.text_style, node.source.text);
        TextStyle value_style = node.source.text_style;
        value_style.horizontal = TextAlign::End;
        text_part(output, node, WidgetPart::CurrentValue, visual_state, node.source.stratum, index,
                  geometry.value, clip, style, value_style, value);
        TextStyle indicator_style = node.source.text_style;
        indicator_style.horizontal = TextAlign::Center;
        const PartPresentation* indicator = part_for(node, WidgetPart::Indicator, visual_state);
        if (indicator && !indicator->asset.empty())
            image(output, node.source.stratum, index, geometry.indicator, clip, *indicator);
        else
            text(output, node.source.stratum, index, geometry.indicator, clip, style,
                 indicator_style, state.open ? "▲" : "▼");
        return;
    }

    std::string rendered = node.source.text;
    if (node.source.control == ControlKind::TextInput && state.editing)
        rendered = state.edit_text + "|";
    else if (node.source.control == ControlKind::TextInput && !value.empty())
        rendered = value;
    text(output, node.source.stratum, index, geometry.label, clip, style, node.source.text_style,
         std::move(rendered));
}

void compose_popup(std::vector<PaintCommand>& output, const CompiledNode& node,
                   const NodeState& state, NodeIndex index, glayout::Rect anchor,
                   glayout::Rect viewport) {
    if (!state.open || node.source.control != ControlKind::Select) return;
    const PopupGeometry geometry =
        resolve_popup_geometry(node.source, anchor, viewport, state.pending_option);
    if (geometry.options.empty()) return;
    const PresentationState visual_state =
        presentation_state(node.source, state.hovered, state.pressed, state.open, false, {});
    const BoxStyle frame = popup_style(node.source.style.normal);
    visual_part(output, node, WidgetPart::Popup, visual_state, PaintStratum::Overlay, index,
                geometry.frame, viewport, frame);
    for (std::size_t row = 0; row < geometry.options.size(); ++row) {
        const std::size_t option = geometry.first_option + row;
        const bool selected = option == state.pending_option;
        const BoxStyle& style = selected ? node.source.style.focused : node.source.style.normal;
        visual_part(output, node, WidgetPart::Option,
                    selected ? PresentationState::Selected : PresentationState::Normal,
                    PaintStratum::Overlay, index, geometry.options[row], viewport, style);
        glayout::Rect label = geometry.options[row];
        const float padding = std::max(10.0f, node.source.text_style.size * 0.75f);
        label.x += padding;
        label.w = std::max(0.0f, label.w - padding * 2.0f);
        text(output, PaintStratum::Overlay, index, label, viewport, style, node.source.text_style,
             node.source.options[option].label);
    }
}

} // namespace

// Builds renderer-neutral paint for semantic nodes and compound control parts.
void compose(const CompiledView& view, const glayout::GraphRuntime& layout,
             const std::vector<NodeState>& state, NodeIndex focus, const Host& host,
             std::vector<PaintCommand>& output) {
    output.clear();
    output.reserve(view.nodes.size() * 4);
    const glayout::Rect viewport =
        layout.nodes().empty() ? glayout::Rect{} : layout.nodes().front().clip;
    for (NodeIndex index = 0; index < view.nodes.size(); ++index) {
        const CompiledNode& node = view.nodes[index];
        const glayout::ResolvedNode& resolved = layout.nodes()[node.layout_index];
        if (!available(node, resolved, host)) continue;
        const glayout::Rect border =
            displayed_rect(view, state, resolved.border, node.layout_index);
        const glayout::Rect content =
            displayed_rect(view, state, resolved.content, node.layout_index);
        if (!glayout::intersects(border, resolved.clip)) continue;
        const Value bound =
            host.read && !node.source.binding.empty() ? host.read(node.source.binding) : Value{};
        const BoxStyle& style = resolve_style(node.source, state[index], focus == index);
        const PresentationState visual_state =
            presentation_state(node.source, state[index].hovered, state[index].pressed,
                               state[index].open, focus == index, bound);
        visual_part(output, node, WidgetPart::Frame, visual_state, node.source.stratum, index,
                    border, resolved.clip, style);
        if (node.source.control != ControlKind::None &&
            node.source.control != ControlKind::ScrollArea) {
            compose_control(output, node, state[index], index, focus == index, content,
                            resolved.clip, bound);
        } else if (node.source.content != ContentKind::None) {
            PaintKind kind = PaintKind::Text;
            if (node.source.content == ContentKind::Image)
                kind = PaintKind::Image;
            else if (node.source.content == ContentKind::Sprite)
                kind = PaintKind::Sprite;
            else if (node.source.content == ContentKind::Progress)
                kind = PaintKind::Progress;
            else if (node.source.content == ContentKind::CustomSurface)
                kind = PaintKind::CustomSurface;
            output.push_back(PaintCommand{kind,
                                          node.source.stratum,
                                          index,
                                          content,
                                          resolved.clip,
                                          style,
                                          node.source.text_style,
                                          node.source.text,
                                          node.source.asset,
                                          numeric_value(bound),
                                          ImageMode::Stretch,
                                          {255, 255, 255, 255},
                                          1.0f,
                                          8.0f,
                                          {},
                                          1.0f});
        }
        compose_popup(output, node, state[index], index, border, viewport);
    }
    std::stable_sort(output.begin(), output.end(),
                     [](const PaintCommand& left, const PaintCommand& right) {
                         return left.stratum < right.stratum;
                     });
}

} // namespace gview::detail
