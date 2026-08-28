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

- `gview::core`: view compilation, retained control state, directed/local focus,
  S-expression persistence, and renderer-neutral paint commands.
- Optional backend and authoring targets will remain separate so release games
  do not link ImGui or editor state.

GView depends on the standalone `glayout::graph` target. GLayout in turn uses
the standalone `gsexp::gsexp` parser.

## Integration

Sibling development checkouts work without extra configuration:

```cmake
set(GVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/gview)
target_link_libraries(my_game PRIVATE gview::core)
```

If GLayout lives elsewhere, set `GVIEW_GLAYOUT_SOURCE_DIR` before adding GView.

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

## Source assets

`write_views()` stores two top-level S-expression forms in one file:

- `ui_graphs` contains layout-only GLayout data.
- `gview_views` contains presentation, controls, focus scopes, and explicit
  navigation overrides referencing stable layout identities.

Both the C++ builder structs and persisted source compile through
`compile_view()`. An eventual AXL frontend can target the same structs without
becoming a runtime dependency.

## Build

```sh
cmake -S . -B build -DGVIEW_WARN_AS_ERROR=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The complete product and trial acceptance contract is intentionally durable in
[`docs/MASTER_PLAN.md`](docs/MASTER_PLAN.md). Runtime work must follow that
contract rather than narrowing around one demonstration.
