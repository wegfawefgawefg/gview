#include "gview/gview.hpp"

#include <cstdlib>
#include <iostream>
#include <unordered_map>

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

gview::NodeSpec control(std::string id, gview::ControlKind kind, std::string group) {
    gview::NodeSpec node;
    node.layout_id = std::move(id);
    node.content = gview::ContentKind::Text;
    node.control = kind;
    node.focus_group = std::move(group);
    node.focusable = true;
    node.style.normal.fill = {10, 20, 24, 255};
    node.style.focused.fill = {30, 80, 50, 255};
    return node;
}

glayout::GraphNode box(std::string id) {
    glayout::GraphNode node;
    node.id = std::move(id);
    node.size.width = {glayout::LengthKind::Fill, 1.0f};
    node.size.height = {glayout::LengthKind::Pixels, 50.0f};
    return node;
}

gview::View sample_view() {
    gview::View view;
    view.id = "controls";
    view.label = "Controls";
    view.layout.id = "controls_layout";
    view.layout.width = 1280;
    view.layout.height = 720;
    view.layout.root.id = "root";
    view.layout.root.container = glayout::ContainerKind::Column;
    view.layout.root.gap = 8.0f;
    view.layout.root.children = {box("tab_settings"), box("volume"), box("quality"), box("mute")};

    gview::NodeSpec tab = control("tab_settings", gview::ControlKind::Button, "rail");
    tab.activation = gview::ActivationPolicy::OnFocus;
    tab.action = "open_settings";
    tab.text = "Settings";
    view.nodes.push_back(tab);

    gview::NodeSpec slider = control("volume", gview::ControlKind::Slider, "content");
    slider.binding = "volume";
    slider.minimum = 0.0;
    slider.maximum = 1.0;
    slider.step = 0.1;
    view.nodes.push_back(slider);

    gview::NodeSpec select = control("quality", gview::ControlKind::Select, "content");
    select.binding = "quality";
    select.options = {{"low", "Low", std::string("low")},
                      {"high", "High", std::string("high")}};
    view.nodes.push_back(select);

    gview::NodeSpec toggle = control("mute", gview::ControlKind::Toggle, "content");
    toggle.binding = "mute";
    view.nodes.push_back(toggle);

    view.focus_groups = {{"rail", "tab_settings", "", false, true},
                         {"content", "volume", "tab_settings", true, true}};
    view.focus_edges = {{"tab_settings", gview::NavAction::Right, "volume"}};
    return view;
}

struct Model {
    std::unordered_map<std::string, gview::Value> values;
    std::vector<std::string> actions;
};

gview::Host host_for(Model& model) {
    gview::Host host;
    host.read = [&](std::string_view key) { return model.values[std::string(key)]; };
    host.write = [&](std::string_view key, const gview::Value& value) {
        model.values[std::string(key)] = value;
    };
    host.action = [&](std::string_view action, gview::NodeIndex) {
        model.actions.emplace_back(action);
    };
    return host;
}

glayout::ResolveInput resolution() {
    return {glayout::Rect{0.0f, 0.0f, 1280.0f, 720.0f}, {}, nullptr, nullptr};
}

// Verifies controller focus is local, explicit, remembered, and not pointer emulation.
void test_focus_and_controls() {
    gview::CompileResult compiled = gview::compile_view(sample_view());
    require(compiled.ok, "sample view compiles");
    gview::Runtime runtime(std::move(compiled.view));
    Model model{{{"volume", 0.5}, {"quality", std::string("low")}, {"mute", false}}, {}};
    gview::Host host = host_for(model);
    runtime.frame(resolution(), {}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "tab_settings",
            "initial focus begins in authored rail");

    runtime.frame(resolution(), {{}, {gview::NavAction::Right}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "volume",
            "explicit group entry moves into content");
    runtime.frame(resolution(), {{}, {gview::NavAction::Right}, {}}, host);
    require(std::get<double>(model.values["volume"]) == 0.6, "slider changes through semantic right");

    runtime.frame(resolution(), {{}, {gview::NavAction::Down, gview::NavAction::Confirm}, {}}, host);
    require(runtime.state()[runtime.focus()].open, "select opens without cycling");
    runtime.frame(resolution(), {{}, {gview::NavAction::Down}, {}}, host);
    require(std::get<std::string>(model.values["quality"]) == "low",
            "moving inside select does not commit");
    runtime.frame(resolution(), {{}, {gview::NavAction::Back}, {}}, host);
    require(!runtime.state()[runtime.focus()].open, "back cancels open select locally");
    require(std::get<std::string>(model.values["quality"]) == "low", "cancel preserves value");

    runtime.frame(resolution(), {{}, {gview::NavAction::Back}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "tab_settings",
            "back returns to owning group");
}

// Verifies pointer activation and stable-frame paint/layout reuse.
void test_pointer_and_cache() {
    gview::Runtime runtime(gview::compile_view(sample_view()).view);
    Model model{{{"volume", 0.5}, {"quality", std::string("low")}, {"mute", false}}, {}};
    gview::Host host = host_for(model);
    runtime.frame(resolution(), {}, host);
    const std::uint64_t builds = runtime.stats().paint_builds;
    runtime.frame(resolution(), {}, host);
    require(runtime.stats().paint_builds == builds, "clean frame reuses paint list");
    require(runtime.stats().layout_builds == 1, "clean frame reuses layout");

    gview::InputFrame press;
    press.pointer = {10.0f, 190.0f, true, true, false, 0.0f};
    runtime.frame(resolution(), press, host);
    gview::InputFrame release;
    release.pointer = {10.0f, 190.0f, false, false, true, 0.0f};
    runtime.frame(resolution(), release, host);
    require(std::get<bool>(model.values["mute"]), "pointer click toggles bound value");
}

// Verifies C++ authoring and persisted authoring compile to equivalent views.
void test_round_trip() {
    const std::string text = gview::write_views({sample_view()});
    const gview::ParseResult parsed = gview::parse_views(text);
    if (!parsed.ok) {
        for (const glayout::Diagnostic& item : parsed.diagnostics) std::cerr << item.message << '\n';
    }
    require(parsed.ok, "view source round trips");
    require(parsed.views.size() == 1, "one view parses");
    require(parsed.views[0].nodes.size() == 4, "controls survive persistence");
    require(gview::compile_view(parsed.views[0]).ok, "parsed view compiles");
}

} // namespace

int main() {
    test_focus_and_controls();
    test_pointer_and_cache();
    test_round_trip();
    std::cout << "gview tests passed\n";
    return 0;
}
