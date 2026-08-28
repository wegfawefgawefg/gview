#include "gview/io.hpp"

#include "theme_io.hpp"

#include <algorithm>
#include <fstream>
#include <gsexp/sexp.hpp>
#include <sstream>
#include <unordered_map>

namespace gview {
namespace {

void diagnostic(ParseResult& result, glayout::DiagnosticSeverity severity, std::string message) {
    result.diagnostics.push_back(glayout::Diagnostic{severity, std::move(message), 1, 1});
}

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

ContentKind content_kind(std::string_view value) {
    if (value == "text") return ContentKind::Text;
    if (value == "image") return ContentKind::Image;
    if (value == "sprite") return ContentKind::Sprite;
    if (value == "progress") return ContentKind::Progress;
    if (value == "custom_surface") return ContentKind::CustomSurface;
    return ContentKind::None;
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

NavAction nav_action(std::string_view value) {
    if (value == "up") return NavAction::Up;
    if (value == "left") return NavAction::Left;
    if (value == "right") return NavAction::Right;
    if (value == "confirm") return NavAction::Confirm;
    if (value == "back") return NavAction::Back;
    if (value == "tab_previous") return NavAction::TabPrevious;
    if (value == "tab_next") return NavAction::TabNext;
    return NavAction::Down;
}

Color parse_color(gsexp::Node source, Color fallback) {
    if (!source.valid()) return fallback;
    const gsexp::FormView color(source);
    const auto channel = [&](std::string_view key, std::uint8_t value) {
        return static_cast<std::uint8_t>(std::clamp(color.get_int(key).value_or(value), 0, 255));
    };
    return Color{channel("r", fallback.r), channel("g", fallback.g), channel("b", fallback.b),
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

Value parse_value(gsexp::Node source) {
    if (!source.valid()) return {};
    if (source.is_string()) return std::string(source.text());
    if (source.text() == "true") return true;
    if (source.text() == "false") return false;
    if (gsexp::looks_like_integer(source.text())) return static_cast<std::int64_t>(std::stoll(std::string(source.text())));
    if (gsexp::looks_like_float(source.text())) return std::stod(std::string(source.text()));
    return std::string(source.text());
}

void parse_node(gsexp::Node source, NodeSpec& node) {
    const gsexp::FormView form(source);
    node.layout_id = form.get_string("layout").value_or("");
    node.content = content_kind(scalar(form, "content").value_or("none"));
    node.control = control_kind(scalar(form, "control").value_or("none"));
    node.activation = scalar(form, "activation").value_or("manual") == "on_focus"
                          ? ActivationPolicy::OnFocus : ActivationPolicy::Manual;
    node.text = form.get_string("text").value_or("");
    node.asset = form.get_string("asset").value_or("");
    node.binding = form.get_string("binding").value_or("");
    node.action = form.get_string("action").value_or("");
    node.focus_group = form.get_string("focus_group").value_or("");
    node.style_class = form.get_string("style_class").value_or("");
    node.condition = form.get_string("condition").value_or("");
    node.minimum = form.get_float("minimum").value_or(0.0f);
    node.maximum = form.get_float("maximum").value_or(1.0f);
    node.step = form.get_float("step").value_or(0.1f);
    node.selected = boolean(form, "selected", false);
    node.focusable = boolean(form, "focusable", node.control != ControlKind::None);
    node.enabled = boolean(form, "enabled", true);

    const gsexp::Node text_style = form.find("text_style");
    if (text_style.valid()) {
        const gsexp::FormView text(text_style);
        node.text_style.font = text.get_string("font").value_or("default");
        node.text_style.size = text.get_float("size").value_or(16.0f);
        node.text_style.line_height = text.get_float("line_height").value_or(0.0f);
        const std::string horizontal = scalar(text, "horizontal").value_or("start");
        node.text_style.horizontal = horizontal == "center" ? TextAlign::Center
                                   : horizontal == "end" ? TextAlign::End : TextAlign::Start;
        node.text_style.wrap = boolean(text, "wrap", false);
    }
    const gsexp::Node style = form.find("style");
    if (style.valid()) {
        const gsexp::FormView states(style);
        node.style.normal = parse_box(states.find("normal"), node.style.normal);
        node.style.selected = parse_box(states.find("selected"), node.style.normal);
        node.style.hovered = parse_box(states.find("hovered"), node.style.normal);
        node.style.focused = parse_box(states.find("focused"), node.style.hovered);
        node.style.selected_focused =
            parse_box(states.find("selected_focused"), node.style.focused);
        node.style.pressed = parse_box(states.find("pressed"), node.style.focused);
        node.style.disabled = parse_box(states.find("disabled"), node.style.normal);
    }
    const gsexp::Node options = form.find("options");
    if (options.valid()) {
        for (gsexp::Node entry : options.children()) {
            if (!entry.is_list() || !entry.head().is_atom("option")) continue;
            const gsexp::FormView option(entry);
            node.options.push_back(SelectOption{option.get_string("id").value_or(""),
                                                option.get_string("label").value_or(""),
                                                parse_value(option.find_arg("value", 0))});
        }
    }
}

bool parse_view(gsexp::Node source, const std::unordered_map<std::string, glayout::GraphLayout>& layouts,
                View& view, ParseResult& result) {
    const gsexp::FormView form(source);
    view.id = form.get_string("id").value_or("");
    view.label = form.get_string("label").value_or("");
    const std::string layout_id = form.get_string("layout").value_or("");
    const auto layout = layouts.find(layout_id);
    if (view.id.empty() || layout == layouts.end()) {
        diagnostic(result, glayout::DiagnosticSeverity::Error,
                   "view has an empty id or references a missing layout");
        return false;
    }
    view.layout = layout->second;
    const gsexp::Node nodes = form.find("nodes");
    if (nodes.valid()) {
        for (gsexp::Node entry : nodes.children()) {
            if (!entry.is_list() || !entry.head().is_atom("node")) continue;
            NodeSpec node;
            parse_node(entry, node);
            view.nodes.push_back(std::move(node));
        }
    }
    const gsexp::Node groups = form.find("focus_groups");
    if (groups.valid()) {
        for (gsexp::Node entry : groups.children()) {
            if (!entry.is_list() || !entry.head().is_atom("group")) continue;
            const gsexp::FormView group(entry);
            view.focus_groups.push_back(FocusGroup{group.get_string("id").value_or(""),
                                                   group.get_string("entry").value_or(""),
                                                   group.get_string("owner").value_or(""),
                                                   boolean(group, "contain", false),
                                                   boolean(group, "remember", true)});
        }
    }
    const gsexp::Node edges = form.find("focus_edges");
    if (edges.valid()) {
        for (gsexp::Node entry : edges.children()) {
            if (!entry.is_list() || !entry.head().is_atom("edge")) continue;
            const gsexp::FormView edge(entry);
            view.focus_edges.push_back(FocusEdge{edge.get_string("from").value_or(""),
                                                 nav_action(scalar(edge, "action").value_or("down")),
                                                 edge.get_string("to").value_or("")});
        }
    }
    const gsexp::Node group_edges = form.find("focus_group_edges");
    if (group_edges.valid()) {
        for (gsexp::Node entry : group_edges.children()) {
            if (!entry.is_list() || !entry.head().is_atom("edge")) continue;
            const gsexp::FormView edge(entry);
            view.focus_group_edges.push_back(
                FocusGroupEdge{edge.get_string("from").value_or(""),
                               nav_action(scalar(edge, "action").value_or("down")),
                               edge.get_string("to").value_or("")});
        }
    }
    detail::parse_themes(form.find("themes"), view);
    return true;
}

void write_color(std::ostringstream& out, std::string_view name, Color color) {
    out << '(' << name << " (r " << static_cast<int>(color.r) << ") (g "
        << static_cast<int>(color.g) << ") (b " << static_cast<int>(color.b) << ") (a "
        << static_cast<int>(color.a) << "))";
}

void write_box(std::ostringstream& out, std::string_view name, const BoxStyle& box) {
    out << "        (" << name << ' ';
    write_color(out, "fill", box.fill); out << ' ';
    write_color(out, "border", box.border); out << ' ';
    write_color(out, "text", box.text);
    out << " (border_width " << box.border_width << ") (corner_radius " << box.corner_radius
        << ") (opacity " << box.opacity << "))\n";
}

void write_value(std::ostringstream& out, const Value& value) {
    if (const bool* boolean = std::get_if<bool>(&value)) out << (*boolean ? "true" : "false");
    else if (const std::int64_t* integer = std::get_if<std::int64_t>(&value)) out << *integer;
    else if (const double* number = std::get_if<double>(&value)) out << *number;
    else if (const std::string* text = std::get_if<std::string>(&value)) out << gsexp::quote_string(*text);
    else out << "nil";
}

void write_node(std::ostringstream& out, const NodeSpec& node) {
    out << "      (node\n        (layout " << gsexp::quote_string(node.layout_id) << ")\n"
        << "        (content " << to_string(node.content) << ") (control "
        << to_string(node.control) << ") (activation "
        << (node.activation == ActivationPolicy::OnFocus ? "on_focus" : "manual") << ")\n"
        << "        (text " << gsexp::quote_string(node.text) << ") (asset "
        << gsexp::quote_string(node.asset) << ") (binding " << gsexp::quote_string(node.binding)
        << ") (action " << gsexp::quote_string(node.action) << ")\n"
        << "        (focus_group " << gsexp::quote_string(node.focus_group) << ") (condition "
        << gsexp::quote_string(node.condition) << ") (style_class "
        << gsexp::quote_string(node.style_class) << ")\n"
        << "        (minimum " << node.minimum << ") (maximum " << node.maximum << ") (step "
        << node.step << ") (selected " << (node.selected ? "true" : "false")
        << ") (focusable " << (node.focusable ? "true" : "false")
        << ") (enabled " << (node.enabled ? "true" : "false") << ")\n"
        << "        (text_style (font " << gsexp::quote_string(node.text_style.font) << ") (size "
        << node.text_style.size << ") (line_height " << node.text_style.line_height
        << ") (horizontal " << (node.text_style.horizontal == TextAlign::Center ? "center" :
                                  node.text_style.horizontal == TextAlign::End ? "end" : "start")
        << ") (wrap " << (node.text_style.wrap ? "true" : "false") << "))\n"
        << "        (style\n";
    write_box(out, "normal", node.style.normal); write_box(out, "selected", node.style.selected);
    write_box(out, "hovered", node.style.hovered); write_box(out, "focused", node.style.focused);
    write_box(out, "selected_focused", node.style.selected_focused);
    write_box(out, "pressed", node.style.pressed);
    write_box(out, "disabled", node.style.disabled); out << "        )\n";
    if (!node.options.empty()) {
        out << "        (options\n";
        for (const SelectOption& option : node.options) {
            out << "          (option (id " << gsexp::quote_string(option.id) << ") (label "
                << gsexp::quote_string(option.label) << ") (value ";
            write_value(out, option.value); out << "))\n";
        }
        out << "        )\n";
    }
    out << "      )\n";
}

} // namespace

// Parses layout and presentation roots from one readable multi-form asset.
ParseResult parse_views(std::string_view text) {
    ParseResult result;
    const glayout::GraphParseResult graphs = glayout::parse_graphs(text);
    result.diagnostics = graphs.diagnostics;
    std::unordered_map<std::string, glayout::GraphLayout> layouts;
    for (const glayout::GraphLayout& graph : graphs.layouts) layouts.emplace(graph.id, graph);
    const gsexp::ParseResult parsed = gsexp::parse(text);
    if (!parsed.ok) return result;
    gsexp::Node root;
    for (std::size_t index = 0; index < parsed.root_count(); ++index) {
        if (parsed.root(index).is_list() && parsed.root(index).head().is_atom("gview_views")) {
            root = parsed.root(index); break;
        }
    }
    if (!root.valid()) {
        diagnostic(result, glayout::DiagnosticSeverity::Error, "missing gview_views root");
        return result;
    }
    for (gsexp::Node entry : root.children()) {
        if (!entry.is_list() || !entry.head().is_atom("view")) continue;
        View view;
        if (parse_view(entry, layouts, view, result)) result.views.push_back(std::move(view));
    }
    result.ok = graphs.ok;
    for (const glayout::Diagnostic& item : result.diagnostics)
        if (item.severity == glayout::DiagnosticSeverity::Error) result.ok = false;
    return result;
}

std::string write_views(const std::vector<View>& views) {
    std::vector<glayout::GraphLayout> layouts;
    for (const View& view : views) layouts.push_back(view.layout);
    std::ostringstream out;
    out << glayout::write_graphs(layouts) << "\n(gview_views\n";
    for (const View& view : views) {
        out << "  (view\n    (id " << gsexp::quote_string(view.id) << ")\n    (label "
            << gsexp::quote_string(view.label) << ")\n    (layout "
            << gsexp::quote_string(view.layout.id) << ")\n    (nodes\n";
        for (const NodeSpec& node : view.nodes) write_node(out, node);
        out << "    )\n    (focus_groups\n";
        for (const FocusGroup& group : view.focus_groups)
            out << "      (group (id " << gsexp::quote_string(group.id) << ") (entry "
                << gsexp::quote_string(group.entry) << ") (owner " << gsexp::quote_string(group.owner)
                << ") (contain " << (group.contain ? "true" : "false") << ") (remember "
                << (group.remember ? "true" : "false") << "))\n";
        out << "    )\n    (focus_edges\n";
        for (const FocusEdge& edge : view.focus_edges)
            out << "      (edge (from " << gsexp::quote_string(edge.from) << ") (action "
                << to_string(edge.action) << ") (to " << gsexp::quote_string(edge.to) << "))\n";
        out << "    )\n";
        out << "    (focus_group_edges\n";
        for (const FocusGroupEdge& edge : view.focus_group_edges)
            out << "      (edge (from " << gsexp::quote_string(edge.from) << ") (action "
                << to_string(edge.action) << ") (to " << gsexp::quote_string(edge.to)
                << "))\n";
        out << "    )\n";
        detail::write_themes(out, view);
        out << "  )\n";
    }
    out << ")\n";
    return out.str();
}

ParseResult load_view_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ParseResult result;
        diagnostic(result, glayout::DiagnosticSeverity::Error, "failed to open: " + path.string());
        return result;
    }
    std::ostringstream text; text << file.rdbuf(); return parse_views(text.str());
}

bool save_view_file(const std::filesystem::path& path, const std::vector<View>& views) {
    std::error_code error;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream file(path); file << write_views(views); return file.good();
}

} // namespace gview
