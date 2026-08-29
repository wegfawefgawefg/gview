#include "gview/gview.hpp"

#include <algorithm>
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
    gview::PartPresentation slider_track;
    slider_track.part = gview::WidgetPart::Track;
    slider_track.asset = "game:ui/rope-track";
    slider_track.image_mode = gview::ImageMode::Tile;
    gview::WidgetSkin slider_skin;
    slider_skin.control = gview::ControlKind::Slider;
    slider_skin.parts.push_back(slider_track);
    view.themes.push_back(gview::Theme{"test-theme", "", {slider_skin}});
    view.active_theme = "test-theme";
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

// Verifies group containment is an authored choice rather than an ignored flag.
void test_optional_focus_containment() {
    gview::CompileResult compiled = gview::compile_view(sample_view());
    require(compiled.ok, "focus containment view compiles");
    const auto available = [&](gview::NodeIndex index) {
        return compiled.view.nodes[index].source.focusable;
    };
    const gview::NodeIndex tab = compiled.view.indices.at("tab_settings");
    const gview::NodeIndex volume = compiled.view.indices.at("volume");
    glayout::GraphRuntime layout(compiled.view.layout);
    layout.resolve(resolution());
    require(gview::next_focus(compiled.view, layout.nodes(), tab, gview::NavAction::Down,
                              available) == volume,
            "non-contained group can use global geometry");
    require(gview::next_focus(compiled.view, layout.nodes(), volume, gview::NavAction::Up,
                              available) != tab,
            "contained group keeps geometric movement local");
}

// Verifies a tab's owned content cannot steal movement that remains valid in
// the tab strip; entering content is an authored group transition.
void test_owned_scope_does_not_steal_local_movement() {
    gview::View view;
    view.id = "scope-priority";
    view.layout.id = "scope-priority-layout";
    view.layout.width = 1280;
    view.layout.height = 720;
    view.layout.root.id = "root";
    view.layout.root.container = glayout::ContainerKind::Absolute;
    glayout::GraphNode first = box("tab-first");
    first.absolute_rect = {0.05f, 0.05f, 0.2f, 0.1f};
    glayout::GraphNode second = box("tab-second");
    second.absolute_rect = {0.65f, 0.05f, 0.2f, 0.1f};
    glayout::GraphNode detail = box("detail");
    detail.absolute_rect = {0.28f, 0.05f, 0.2f, 0.1f};
    view.layout.root.children = {first, second, detail};
    view.nodes = {control("tab-first", gview::ControlKind::Button, "tabs"),
                  control("tab-second", gview::ControlKind::Button, "tabs"),
                  control("detail", gview::ControlKind::Button, "detail")};
    view.focus_groups = {{"tabs", "tab-first", "", true, true},
                         {"detail", "detail", "tab-first", true, true}};
    view.focus_group_edges = {{"tabs", gview::NavAction::Down, "detail"}};
    gview::CompileResult compiled = gview::compile_view(view);
    require(compiled.ok, "nested scope priority view compiles");
    glayout::GraphRuntime layout(compiled.view.layout);
    layout.resolve(resolution());
    const auto available = [&](gview::NodeIndex index) {
        return compiled.view.nodes[index].source.focusable;
    };
    require(gview::next_focus(compiled.view, layout.nodes(),
                              compiled.view.indices.at("tab-first"), gview::NavAction::Right,
                              available) == compiled.view.indices.at("tab-second"),
            "local tab movement wins over an owned content entry");
}

// Verifies scope links remember a dynamic member instead of persisting its ID.
void test_group_links_and_memory() {
    gview::View view = sample_view();
    view.focus_edges.clear();
    view.focus_groups[1].entry.clear();
    view.focus_group_edges = {{"rail", gview::NavAction::Right, "content"},
                              {"content", gview::NavAction::Left, "rail"}};
    gview::CompileResult compiled = gview::compile_view(view);
    require(compiled.ok, "automatic group entry and scope links compile");
    gview::Runtime runtime(std::move(compiled.view));
    Model model{{{"volume", 0.5}, {"quality", std::string("low")}, {"mute", false}}, {}};
    gview::Host host = host_for(model);
    runtime.frame(resolution(), {}, host);
    runtime.frame(resolution(), {{}, {gview::NavAction::Right}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "volume",
            "scope link enters its automatic first member");
    runtime.frame(resolution(), {{}, {gview::NavAction::Down, gview::NavAction::Down}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "mute",
            "local movement reaches another materialized member");
    runtime.frame(resolution(), {{}, {gview::NavAction::Left}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "tab_settings",
            "scope link exits without a node-specific edge");
    runtime.frame(resolution(), {{}, {gview::NavAction::Right}, {}}, host);
    require(runtime.view().nodes[runtime.focus()].source.layout_id == "mute",
            "returning scope link restores the exact remembered member");
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
    const auto themed_track = std::find_if(runtime.paint().begin(), runtime.paint().end(),
                                           [](const gview::PaintCommand& command) {
                                               return command.asset == "game:ui/rope-track" &&
                                                      command.image_mode ==
                                                          gview::ImageMode::Tile;
                                           });
    require(themed_track != runtime.paint().end(),
            "compiled widget skin emits renderer-neutral asset part");
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

// Verifies compound controls reserve distinct semantic slots and portal space.
void test_widget_geometry() {
    gview::NodeSpec slider = control("volume", gview::ControlKind::Slider, "content");
    slider.text_style.size = 16.0f;
    const gview::WidgetGeometry slider_geometry =
        gview::resolve_widget_geometry(slider, {20.0f, 30.0f, 400.0f, 60.0f}, 0.5);
    require(slider_geometry.label.y + slider_geometry.label.h <= slider_geometry.track.y,
            "slider label does not overlap track");
    require(slider_geometry.thumb.x + slider_geometry.thumb.w * 0.5f == 220.0f,
            "slider thumb follows value ratio");
    require(slider_geometry.thumb.y >= 30.0f &&
                slider_geometry.thumb.y + slider_geometry.thumb.h <= 90.0f,
            "slider theme parts remain fully inside the control");

    gview::NodeSpec select = control("quality", gview::ControlKind::Select, "content");
    select.text_style.size = 16.0f;
    select.options = {{"a", "Alpha", std::string("a")},
                      {"b", "Beta", std::string("b")},
                      {"c", "Gamma", std::string("c")}};
    const gview::PopupGeometry popup = gview::resolve_popup_geometry(
        select, {900.0f, 680.0f, 300.0f, 40.0f}, {0.0f, 0.0f, 1280.0f, 720.0f}, 1);
    require(popup.frame.y < 680.0f, "popup flips above an obstructed anchor");
    require(popup.frame.x + popup.frame.w <= 1280.0f, "popup stays inside viewport");
    require(popup.options.size() == 3, "popup retains visible options");
}

// Verifies C++ authoring and persisted authoring compile to equivalent views.
void test_round_trip() {
    gview::View view = sample_view();
    view.focus_group_edges.push_back({"rail", gview::NavAction::Right, "content"});
    const std::string text = gview::write_views({view});
    const gview::ParseResult parsed = gview::parse_views(text);
    if (!parsed.ok) {
        for (const glayout::Diagnostic& item : parsed.diagnostics)
            std::cerr << item.message << '\n';
    }
    require(parsed.ok, "view source round trips");
    require(parsed.views.size() == 1, "one view parses");
    require(parsed.views[0].nodes.size() == 4, "controls survive persistence");
    require(parsed.views[0].themes.size() == 1 &&
                parsed.views[0].themes[0].widgets[0].parts[0].asset ==
                    "game:ui/rope-track",
            "asset-skinned compound controls survive persistence");
    require(parsed.views[0].focus_group_edges == view.focus_group_edges,
            "group-level navigation survives persistence");
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
    require(session.connect_groups("rail", gview::NavAction::Right, "content"),
            "authoring adds a data-independent group edge");
    require(session.connect("volume", gview::NavAction::Right, "quality"),
            "authoring replaces a direction without ambiguity");
    require(session.view().focus_edges.back().to == "quality", "replacement target is retained");
    require(session.dirty() && session.can_undo(), "authoring records history");
    require(session.undo(), "authoring undo succeeds");
    require(session.view().focus_edges.back().to == "extra", "undo restores replaced edge");
    require(session.redo(), "authoring redo succeeds");
    require(session.duplicate("extra", "extra-copy"), "authoring duplicates presentation tree");
    require(session.remove("extra-copy"), "authoring removes presentation tree");
    const std::optional<gview::AuthoringFragment> copied = session.copy("extra");
    require(copied.has_value(), "authoring copies a synchronized fragment");
    require(session.paste(*copied, "root", "extra-pasted"), "authoring pastes a fragment");
    require(glayout::find_graph_node(session.view().layout, "extra-pasted") != nullptr,
            "pasted layout receives a new identity");
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "gview-authoring-test.sexp";
    require(session.save_as(path), "authoring saves runtime format");
    require(session.reload(), "authoring reloads runtime format");
    std::filesystem::remove(path);
}

// Verifies display coverage and invalid theme inheritance fail at compile time.
void test_authoring_validation() {
    const std::vector<gview::PreviewPreset>& presets = gview::preview_presets();
    require(presets.size() == 36,
            "display simulator retains all legacy device and resolution presets");
    const auto phone = std::find_if(presets.begin(), presets.end(), [](const auto& preset) {
        return std::string_view(preset.label).find("1179x2556") != std::string_view::npos;
    });
    require(phone != presets.end() && phone->width == 393 && phone->height == 852 &&
                phone->output_width == 1179 && phone->output_height == 2556 &&
                phone->device_pixel_ratio == 3.0f,
            "mobile preset separates logical points, physical pixels, and density");
    gview::View view = sample_view();
    view.themes.push_back({"cycle", "trial", {}});
    view.themes[0].extends = "cycle";
    require(!gview::compile_view(view).ok, "theme inheritance cycles are diagnosed");
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
    test_optional_focus_containment();
    test_owned_scope_does_not_steal_local_movement();
    test_group_links_and_memory();
    test_focus_overrides_and_diagnostics();
    test_pointer_and_cache();
    test_widget_geometry();
    test_round_trip();
    test_authoring_session();
    test_authoring_validation();
    test_virtual_collection();
    std::cout << "gview tests passed\n";
    return 0;
}
