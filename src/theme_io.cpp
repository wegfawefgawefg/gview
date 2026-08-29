#include "theme_io.hpp"

#include <algorithm>

namespace gview::detail {
namespace {

std::optional<std::string> scalar(gsexp::FormView form, std::string_view key) {
    const gsexp::Node value = form.find_arg(key, 0);
    if (!value.valid() || value.is_list()) return std::nullopt;
    return std::string(value.text());
}

bool boolean(gsexp::FormView form, std::string_view key, bool fallback) {
    const auto value = scalar(form, key);
    if (!value) return fallback;
    return *value == "true" || *value == "on" || *value == "yes" || *value == "1";
}

Color parse_color(gsexp::Node source, Color fallback) {
    if (!source.valid()) return fallback;
    const gsexp::FormView color(source);
    const auto channel = [&](std::string_view key, std::uint8_t value) {
        return static_cast<std::uint8_t>(std::clamp(color.get_int(key).value_or(value), 0, 255));
    };
    return {channel("r", fallback.r), channel("g", fallback.g), channel("b", fallback.b),
            channel("a", fallback.a)};
}

BoxStyle parse_box(gsexp::Node source, BoxStyle fallback) {
    if (!source.valid()) return fallback;
    const gsexp::FormView box(source);
    fallback.fill = parse_color(box.find("fill"), fallback.fill);
    fallback.border = parse_color(box.find("border"), fallback.border);
    fallback.text = parse_color(box.find("text"), fallback.text);
    fallback.border_width = box.get_float("border_width").value_or(fallback.border_width);
    fallback.corner_radius = box.get_float("corner_radius").value_or(fallback.corner_radius);
    fallback.opacity = box.get_float("opacity").value_or(fallback.opacity);
    return fallback;
}

ControlKind control_kind(std::string_view value) {
    if (value == "button") return ControlKind::Button;
    if (value == "toggle") return ControlKind::Toggle;
    if (value == "slider") return ControlKind::Slider;
    if (value == "select") return ControlKind::Select;
    if (value == "text_input") return ControlKind::TextInput;
    if (value == "scroll_area") return ControlKind::ScrollArea;
    return ControlKind::None;
}

WidgetPart widget_part(std::string_view value) {
    if (value == "label") return WidgetPart::Label;
    if (value == "description") return WidgetPart::Description;
    if (value == "value") return WidgetPart::CurrentValue;
    if (value == "track") return WidgetPart::Track;
    if (value == "fill") return WidgetPart::Fill;
    if (value == "thumb") return WidgetPart::Thumb;
    if (value == "indicator") return WidgetPart::Indicator;
    if (value == "popup") return WidgetPart::Popup;
    if (value == "option") return WidgetPart::Option;
    return WidgetPart::Frame;
}

PresentationState presentation_state(std::string_view value) {
    if (value == "selected") return PresentationState::Selected;
    if (value == "hovered") return PresentationState::Hovered;
    if (value == "focused") return PresentationState::Focused;
    if (value == "selected_focused") return PresentationState::SelectedFocused;
    if (value == "pressed") return PresentationState::Pressed;
    if (value == "disabled") return PresentationState::Disabled;
    if (value == "open") return PresentationState::Open;
    if (value == "on") return PresentationState::On;
    if (value == "off") return PresentationState::Off;
    return PresentationState::Normal;
}

ImageMode image_mode(std::string_view value) {
    if (value == "natural") return ImageMode::Natural;
    if (value == "contain") return ImageMode::Contain;
    if (value == "cover") return ImageMode::Cover;
    if (value == "tile") return ImageMode::Tile;
    if (value == "nine_slice") return ImageMode::NineSlice;
    return ImageMode::Stretch;
}

PartPresentation parse_part(gsexp::Node source) {
    const gsexp::FormView form(source);
    PartPresentation part;
    part.part = widget_part(scalar(form, "id").value_or("frame"));
    part.state = presentation_state(scalar(form, "state").value_or("normal"));
    part.asset = form.get_string("asset").value_or("");
    part.image_mode = image_mode(scalar(form, "image_mode").value_or("stretch"));
    part.opacity = form.get_float("opacity").value_or(1.0f);
    part.slice = form.get_float("slice").value_or(8.0f);
    const gsexp::Node margins = form.find("slice_margins");
    if (margins.valid()) {
        const gsexp::FormView values(margins);
        part.slice_margins.left = values.get_float("left").value_or(part.slice);
        part.slice_margins.top = values.get_float("top").value_or(part.slice);
        part.slice_margins.right = values.get_float("right").value_or(part.slice);
        part.slice_margins.bottom = values.get_float("bottom").value_or(part.slice);
    }
    part.slice_scale = form.get_float("slice_scale").value_or(1.0f);
    part.tint = parse_color(form.find("tint"), part.tint);
    part.override_box = boolean(form, "override_box", form.find("box").valid());
    part.box = parse_box(form.find("box"), part.box);
    return part;
}

void write_color(std::ostringstream& out, std::string_view name, Color color) {
    out << '(' << name << " (r " << static_cast<int>(color.r) << ") (g "
        << static_cast<int>(color.g) << ") (b " << static_cast<int>(color.b) << ") (a "
        << static_cast<int>(color.a) << "))";
}

void write_box(std::ostringstream& out, const BoxStyle& box) {
    out << " (box ";
    write_color(out, "fill", box.fill);
    out << ' ';
    write_color(out, "border", box.border);
    out << ' ';
    write_color(out, "text", box.text);
    out << " (border_width " << box.border_width << ") (corner_radius " << box.corner_radius
        << ") (opacity " << box.opacity << "))";
}

} // namespace

// Parses reusable theme inheritance and widget-part overrides from one view.
void parse_themes(gsexp::Node source, View& view) {
    if (!source.valid()) return;
    const gsexp::FormView root(source);
    view.active_theme = root.get_string("active").value_or("");
    for (gsexp::Node theme_node : source.children()) {
        if (!theme_node.is_list() || !theme_node.head().is_atom("theme")) continue;
        const gsexp::FormView theme_form(theme_node);
        Theme theme;
        theme.id = theme_form.get_string("id").value_or("");
        theme.extends = theme_form.get_string("extends").value_or("");
        for (gsexp::Node widget_node : theme_node.children()) {
            if (!widget_node.is_list() || !widget_node.head().is_atom("widget")) continue;
            const gsexp::FormView widget_form(widget_node);
            WidgetSkin widget;
            widget.control = control_kind(scalar(widget_form, "control").value_or("none"));
            widget.any_control = boolean(widget_form, "any_control", false);
            widget.style_class = widget_form.get_string("class").value_or("");
            widget.node_id = widget_form.get_string("node").value_or("");
            for (gsexp::Node part_node : widget_node.children()) {
                if (part_node.is_list() && part_node.head().is_atom("part"))
                    widget.parts.push_back(parse_part(part_node));
            }
            theme.widgets.push_back(std::move(widget));
        }
        view.themes.push_back(std::move(theme));
    }
}

// Writes the normal hot-reload source representation for presentation recipes.
void write_themes(std::ostringstream& out, const View& view) {
    if (view.themes.empty() && view.active_theme.empty()) return;
    out << "    (themes\n      (active " << gsexp::quote_string(view.active_theme) << ")\n";
    for (const Theme& theme : view.themes) {
        out << "      (theme (id " << gsexp::quote_string(theme.id) << ") (extends "
            << gsexp::quote_string(theme.extends) << ")\n";
        for (const WidgetSkin& widget : theme.widgets) {
            out << "        (widget (control " << to_string(widget.control) << ") (any_control "
                << (widget.any_control ? "true" : "false") << ") (class "
                << gsexp::quote_string(widget.style_class) << ") (node "
                << gsexp::quote_string(widget.node_id) << ")\n";
            for (const PartPresentation& part : widget.parts) {
                out << "          (part (id " << to_string(part.part) << ") (state "
                    << to_string(part.state) << ") (asset " << gsexp::quote_string(part.asset)
                    << ") (image_mode " << to_string(part.image_mode) << ") (opacity "
                    << part.opacity << ") (slice " << part.slice << ") (slice_scale "
                    << part.slice_scale << ") (override_box "
                    << (part.override_box ? "true" : "false") << ") ";
                if (part.slice_margins.left >= 0.0f) {
                    out << "(slice_margins (left " << part.slice_margins.left << ") (top "
                        << part.slice_margins.top << ") (right " << part.slice_margins.right
                        << ") (bottom " << part.slice_margins.bottom << ")) ";
                }
                write_color(out, "tint", part.tint);
                if (part.override_box) write_box(out, part.box);
                out << ")\n";
            }
            out << "        )\n";
        }
        out << "      )\n";
    }
    out << "    )\n";
}

} // namespace gview::detail
