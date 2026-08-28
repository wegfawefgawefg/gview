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
    select.options = {{"low", "Low", std::string("low")}, {"high", "High", std::string("high")}};
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

// Verifies controller focus is local, explicit, remembered, and not pointer
// emulation.
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
    require(std::get<double>(model.values["volume"]) == 0.6,
            "slider changes through semantic right");

    runtime.frame(resolution(), {{}, {gview::NavAction::Down, gview::NavAction::Confirm}, {}},
                  host);
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

// Verifies non-directional overrides and graph diagnostics use runtime
// semantics.
void test_focus_overrides_and_diagnostics() {
    gview::View view = sample_view();
    view.focus_edges.push_back({"tab_settings", gview::NavAction::Confirm, "quality"});
    view.focus_edges.push_back({"mute", gview::NavAction::Back, "quality"});
    gview::CompileResult compiled = gview::compile_view(view);
    require(compiled.ok, "focus override view compiles");
    gview::Runtime runtime(std::move(compiled.view));
    Model model{{{"volume", 0.5}, {"quality", std::string("low")}, {"mute", false}}, {}};
    gview::Host host = host_for(model);
    runtime.frame(resolution(), {}, host);
    require(gview::analyze_focus_graph(runtime.view(), runtime.geometry()).empty(),
            "focus diagnostics see every reachable target");
    runtime.frame(resolution(), {{}, {gview::NavAction::Confirm}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "quality",
            "confirm override enters authored destination");
    runtime.frame(resolution(), {{}, {gview::NavAction::Down, gview::NavAction::Back}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "quality",
            "back override precedes group exit");
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
        for (const glayout::Diagnostic& item : parsed.diagnostics)
            std::cerr << item.message << '\n';
    }
    require(parsed.ok, "view source round trips");
    require(parsed.views.size() == 1, "one view parses");
    require(parsed.views[0].nodes.size() == 4, "controls survive persistence");
    require(gview::compile_view(parsed.views[0]).ok, "parsed view compiles");
}

// Verifies live structural authoring, history, focus edges, and persisted
// reload.
void test_authoring_session() {
    gview::AuthoringSession session;
    session.open(sample_view());
    glayout::GraphNode layout = box("extra");
    gview::NodeSpec presentation = control("extra", gview::ControlKind::Button, "content");
    require(session.add("root", layout, presentation), "authoring adds synchronized node");
    require(session.connect("volume", gview::NavAction::Right, "extra"),
            "authoring adds explicit focus edge");
    require(session.connect("volume", gview::NavAction::Right, "quality"),
            "authoring replaces a direction without ambiguity");
    require(session.view().focus_edges.back().to == "quality", "replacement target is retained");
    require(session.dirty() && session.can_undo(), "authoring records history");
    require(session.undo(), "authoring undo succeeds");
    require(session.view().focus_edges.back().to == "extra", "undo restores replaced edge");
    require(session.redo(), "authoring redo succeeds");
    require(session.duplicate("extra", "extra-copy"), "authoring duplicates presentation tree");
    require(session.remove("extra-copy"), "authoring removes presentation tree");
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gview-authoring-test.sexp";
    require(session.save_as(path), "authoring saves runtime format");
    require(session.reload(), "authoring reloads runtime format");
    std::filesystem::remove(path);
}

// Verifies large host collections can preserve identities while materializing a
// small window.
void test_virtual_collection() {
    const gview::VirtualRange range = gview::virtual_range(1000, 40.0f, 240.0f, 400.0f, 2);
    require(range.first == 8 && range.last == 19,
            "virtual range includes visible rows and overscan");
    require(range.leading_extent == 320.0f && range.trailing_extent == 39240.0f,
            "virtual spacers preserve full scroll extent");
    require(gview::collection_item_id("mods", "weather 2.0") == "mods/weather-2-0",
            "collection identity is stable and source safe");
}

} // namespace

int main() {
    test_focus_and_controls();
    test_focus_overrides_and_diagnostics();
    test_pointer_and_cache();
    test_round_trip();
    test_authoring_session();
    test_virtual_collection();
    std::cout << "gview tests passed\n";
    return 0;
}
