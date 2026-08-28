# gview

`gview` is a lightweight C++ game-UI presentation and interaction
library built on the independent `glayout` geometry engine. It is intended for
menus, HUDs, inventories, dialogue, loading screens, world-space indicators,
custom surfaces, and other game interfaces without requiring a DOM or web
runtime.

The project is being developed against the complete Gubsy UI design trial. The
canonical scope and acceptance contract are in
[`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md).

The renderer-neutral runtime compiles C++ or S-expression view sources to dense
tables. It emits clipped paint commands, keeps controller focus distinct from
pointer hover, and provides real button, toggle, slider, select, text-input, and
scroll-area state contracts. GLayout remains responsible for geometry.

## Targets

- `gview::core`: view/theme compilation, retained control state, generated local
  and group-aware focus, S-expression persistence, compound asset-skinned
  controls, virtual-collection helpers, and renderer-neutral paint commands.
- `gview::authoring`: optional editable source sessions, atomic persistence,
  and snapshot undo/redo without any ImGui dependency.
- `gview::sdl3`: optional SDL3 renderer with cached FreeType/HarfBuzz text,
  images, clipping, and host-registered custom surfaces.
- `gview::imgui`: optional live hierarchy, simulation, overlay, performance,
  and directed-focus-graph authoring tools.

The core never depends on SDL or ImGui. A release build can link only
`gview::core`, or add its chosen renderer, without retaining authoring state.
Editable sessions are built when `GVIEW_BUILD_AUTHORING`, tests, or the ImGui
suite requests them; otherwise that target and implementation are absent.

GView depends on the standalone `glayout::graph` target. GLayout in turn uses
the standalone `gsexp::gsexp` parser.

## Integration

An explicit sibling checkout is useful during development:

```cmake
set(GVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GVIEW_GLAYOUT_SOURCE_DIR /path/to/glayout CACHE PATH "" FORCE)
add_subdirectory(third_party/gview)
target_link_libraries(my_game PRIVATE gview::core)
```

When no GLayout target or source path is supplied, CMake fetches the pinned
standalone revision. SDL and ImGui remain opt-in through
`GVIEW_WITH_SDL3_RENDERER` and `GVIEW_WITH_IMGUI`.

## Runtime model

Game code owns model values and actions. The host adapter exposes typed reads,
writes, conditions, actions, and text measurement. Input is semantic: a
controller sends `NavAction::Down`, for example, rather than moving a synthetic
mouse pointer.

```cpp
gview::CompileResult compiled = gview::compile_view(authored_view);
gview::Runtime runtime(std::move(compiled.view));

gview::Host host;
host.read = read_game_value;
host.write = write_game_value;
host.action = dispatch_game_action;
host.measure = measure_text_or_asset;

runtime.frame(resolve_input, input_frame, host);
for (const gview::PaintCommand& command : runtime.paint()) {
    renderer.consume(command);
}
```

Stable frames reuse both resolved GLayout geometry and the GView paint list.
The host increments `Host::revision` or calls `invalidate_paint()` when an
external value changes without passing through a GView control.

`Runtime::owned_bytes()` reports an estimate of library-owned view, layout,
control, focus, and paint storage. It deliberately excludes host textures,
font atlases, renderer drivers, and swapchains; whole-process RSS answers a
different question.

## Collections and custom content

Game data remains authoritative in C++. `collection_item_id()` derives a stable
node identity from a collection and game-owned key. `virtual_range()` computes
the visible interval plus leading/trailing spacer extents for large uniform
lists or grids. The host materializes that interval from one reusable template;
focus relationships are generated geometrically, so item edges are not stored
one by one.

`CustomSurface` nodes emit normal clipped paint commands. An SDL host can
register a callback by asset ID with `Sdl3Renderer::register_surface()`, allowing
native game, render-target, map, character, or world-preview drawing inside a
GView composition. GView therefore does not constrain interfaces to DOM-like
rectangles even though rectangles define placement and clipping.

## Source assets

`write_views()` stores two top-level S-expression forms in one file:

- `ui_graphs` contains layout-only GLayout data.
- `gview_views` contains presentation, controls, focus scopes, and explicit
  navigation overrides referencing stable layout identities.

Both the C++ builder structs and persisted source compile through
`compile_view()`. An eventual AXL frontend can target the same structs without
becoming a runtime dependency.

## Live authoring

`AuthoringSession` edits the same `View` loaded by production code and saves the
same S-expression format atomically. It supports create, duplicate, delete,
reparent, cut/copy/paste, undo/redo, reload, explicit node edges, and group
entry/exit links. The optional ImGui suite surrounds the actual native canvas
rather than replacing it with a detached schematic:

- Explicit Test/Edit modes and clean, layout, focus, and combined overlays.
- Direct move and eight-handle resize, multi-select, grid/sibling snapping,
  guides, nudge, constraint-aware reorder, and numeric property editing.
- Thirty-six display presets with separate logical/physical size, device pixel
  ratio, UI scale, form factor, safe area, fit/sampling, zoom, and pan.
- Layout, presentation, control, binding, action, and style property editing.
- Safely staged node and group Up/Down/Left/Right/Confirm/Back/bumper links.
- Reachability diagnostics using the runtime navigation rules.
- Live frame, node, command, and owned-memory telemetry.

The complete working integration is the `gview/` entry in
[`gubsy-ui-kit-trials`](https://github.com/wegfawefgawefg/gubsy-ui-kit-trials).

## Build

```sh
cmake -S . -B build -DGVIEW_WARN_AS_ERROR=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The complete product and trial acceptance contract is intentionally durable in
[`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md). Runtime work must follow that
contract rather than narrowing around one demonstration.
