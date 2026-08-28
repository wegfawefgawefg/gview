# GView master plan

## Purpose

GView will be a production-oriented, lightweight game-UI composition,
presentation, and interaction library. It will use GLayout for geometry while
remaining unopinionated about game content and custom rendering.

The first proof is not a reduced technology sample. GView must recreate the
complete behavior and content of the established Vue and RmlUi Gubsy shell
trials, then prove that the same runtime also suits non-menu game UI.

This document is the durable contract for the project. Compaction, iteration,
and implementation discoveries must not silently narrow it.

## Product model

The Gubsy ecosystem is a composition of small, independently useful libraries:

```text
gsexp
  S-expression parsing and writing

glayout
  authored and computed spatial relationships

gview
  content presentation, interaction, controls, and authoring tools

gubsy
  packaged engine integration and standard developer tooling
```

Gubsy consumes these projects as dependencies. It must not carry drifting
copies of their implementations.

## Responsibility boundary

GLayout owns:

- Stable authored layout identity.
- Layout variants for resolution, aspect ratio, and form factor.
- Nested coordinate systems and container geometry.
- Rows, columns, grids, anchors, constraints, gaps, and alignment.
- Intrinsic measurement requests and resolved rectangles/transforms.
- Content extents and clip/mask geometry.
- Dirty-layout dependency resolution.
- Renderer-free layout editing operations and persistence.

GView owns:

- Presentation nodes and content slots.
- Text, images, sprites, render targets, and custom surfaces.
- Collection repetition, virtualization, and scroll state.
- Hit testing, focus scopes, directed navigation, and input policy.
- Buttons, toggles, sliders, selects, text input, and composite controls.
- Actions, Gubsy event adapters, view state, and conditional presentation.
- Paint order, overlays, modal behavior, and renderer command generation.
- Optional ImGui authoring, inspection, and navigation-graph tools.

Game code owns:

- Semantic models and authoritative state.
- Meaningful text, assets, collections, and commands.
- Gameplay effects of UI actions.
- Custom 2D/3D rendering submitted through surface contracts.
- Save, profile, mod, network, and gameplay policy.

## Authoring model

S-expression assets and a C++ builder compile to the same validated runtime
representation. A later AXL frontend may compile to that representation, but
AXL compatibility is not a prerequisite for the first production proof.

The S-expression format is a real persisted source format, not a debug dump.
The live editor must save normal source assets that can be loaded again without
manual translation.

Layout data and presentation data may be separate logical sections even when a
single view file embeds both for convenience. The implementation must preserve
their ownership boundary.

Game content is normally supplied from C++. Constant content may be authored in
the view source. Text and image content participate in layout only through an
intrinsic measurement contract.

## Runtime representation

Authored UUIDs and readable aliases provide durable identity. Compilation maps
them to dense indices and flat tables for runtime use.

The compiled representation should contain:

- Dense layout and presentation node tables.
- Parent/child ranges and stable source identity.
- Container and constraint records.
- Content-provider and measurement slots.
- Conditional state and variant records.
- Collection templates and virtualized materialization metadata.
- Focus scopes, generated relationships, and explicit edge overrides.
- Action bindings and typed value/control contracts.
- Paint strata, local order, clips, masks, and custom-surface records.
- Source spans and diagnostics for authoring failures.

Stable frames must not rebuild the graph, rerun layout, or allocate without a
dirty cause. Dirty changes should invalidate the smallest useful domain.

## Layout capabilities

The reusable layout vocabulary must cover:

- Absolute authored boxes for compatibility and unusual composition.
- Nested layout templates.
- Row, column, grid, stack, and overlay containers.
- Fixed, proportional, intrinsic, fill, min/max, and aspect-constrained sizes.
- Parent, sibling, safe-area, and named-node anchors.
- Padding, gaps, alignment, and distribution.
- Repeated cell templates driven by a runtime count.
- Scroll viewport and content-extent geometry.
- Responsive variants selected by resolution, aspect, form factor, and DPI.
- Coordinate spaces for screen, safe area, render target, and custom surfaces.
- Deterministic cycle and invalid-constraint diagnostics.

The system must avoid becoming a general arbitrary constraint solver. A small,
predictable vocabulary is preferable to open-ended equations.

## Presentation capabilities

A resolved region may host:

- No content; it may be an anchor or grouping node.
- Measured and shaped text.
- Images, sprites, and nine-slice visuals.
- Progress/bars and simple generated geometry.
- A collection item template.
- A nested composed view.
- A conventional control.
- A render target or native custom surface.
- A game-provided 2D or 3D drawing callback.

Tree order determines ordinary painting. A small number of explicit strata
handle background, content, overlay, modal, tooltip, and prompt presentation.
Local portals may paint in another stratum while remaining anchored to their
logical source node.

Rectangular clipping should use scissor operations. Rounded or arbitrary masks
may use renderer-provided stencil, mask texture, or offscreen operations.

## Text quality

Text is a first-class subsystem, not a placeholder rectangle.

- Measurement uses real advances, ascenders, descenders, line gaps, and baselines.
- Horizontal and vertical centering use font metrics.
- Text wraps consistently against the proposed width.
- DPI and output scaling do not make small text blurry or aliased.
- Glyph runs and atlases are cached.
- The runtime supports fallback fonts or reports missing glyphs safely.
- The layout core remains independent from a particular shaping/font library.

The first backend should reuse a proven Gubsy text path or correctly integrate
FreeType/HarfBuzz. A deliberately poor bitmap-font shortcut is not acceptable.

## Input and focus model

Pointer and controller interaction are different policies over the same
interactive nodes. Controller input must not merely impersonate a mouse.

The focus system supports:

- Individual focus nodes.
- Focus groups and nested scopes.
- Generated row, column, grid, and list relationships.
- Explicit directional overrides.
- Group entry and exit edges.
- Remembered focus per group and view.
- Modal containment and cancel/back behavior.
- Tab and bumper transitions.
- Dynamic collection instances with stable template identity.
- Disabled, hidden, removed, and temporarily unavailable targets.

Back normally leaves the current local mode or returns focus to the owning
group before navigating to a top-level destination. Dropdown cancellation and
other captured-control behavior occur before screen-level back.

## Navigation graph editor

The optional live tool must display focus nodes, scopes, generated edges, and
explicit overrides over the running interface.

An author can:

1. Select a node or group.
2. Select Up, Down, Left, Right, Back, tab, or another navigation action.
3. Click a destination node or group.
4. Test the new edge immediately with a controller.
5. Remove, replace, undo, redo, and save it.

The editor validates unreachable nodes, missing destinations, trapped scopes,
invalid modal exits, and references removed by the active variant. Cycles are
not inherently errors because normal menus intentionally wrap or return.

Generated collection relationships must not require one persisted edge per
item. The graph stores container rules and only exceptional overrides.

## Integrated authoring suite

Every Gubsy game should be able to register the authoring suite once and gain:

- Active view and nested-state selection.
- Fake data-scenario selection.
- Independent window and internal-render resolutions.
- Resolution/aspect presets.
- DPI/UI scale simulation.
- Desktop, tablet, phone, and explicit variant forcing.
- Safe-area editing.
- Fit, stretch, and pixel-perfect scale modes.
- Nearest and linear sampling.
- Preview zoom and pan.
- Live layout/view reload.
- Layout hierarchy, clip, baseline, dirty, and focus overlays.
- Runtime performance and allocation counters.

The layout editor supports:

- Select, multi-select, drag, resize, nudge, and snapping.
- Create, duplicate, delete, and reparent.
- Row, column, grid, stack, scroll, overlay, text, image, and surface nodes.
- Numeric geometry, constraint, padding, gap, and alignment editing.
- Layout creation and duplication across variants.
- Copying selected changes between variants.
- Undo/redo for every persisted mutation.
- Atomic save, restore, and live reload.
- Source identity and diagnostic inspection.

ImGui and all authoring state remain optional and absent from release targets.

## Reference trial

The GView entry in `gubsy-ui-kit-trials` uses the Vue and RmlUi versions as the
behavior and content references. It must implement rather than imitate:

- Play, Players, Settings, Controls, Progress, and Mods destinations.
- Continue and new-expedition state differences.
- Quest/checkpoint selection.
- Expedition settings and mod-contributed settings.
- Session mod dependency management and catalog installation flows.
- Player profiles, local players, and multi-device assignment.
- Binding capture and manual device/input browsing.
- Saves/checkpoints/profile metadata distinctions.
- Real controls, overlays, lists, grids, images, and scrolling.
- Mouse, keyboard, and full controller reachability.
- 1280x720 and 1920x1080 production layouts.
- Compact layout evidence where appropriate.

The trial also includes at least one non-menu proof such as a HUD or inventory
overlay with a custom game-rendered surface.

No dead buttons, simulated sliders, click-to-cycle fake selects, unreachable
controls, accidental outer scrolling, or placeholder image omissions count as
complete.

## Generality rule

Every proposed library primitive must be classified:

1. Reusable geometry belongs to GLayout.
2. Reusable presentation or interaction belongs to GView.
3. Gubsy engine hosting belongs to Gubsy.
4. Splonks/reference-specific meaning remains in the trial.
5. Unusual rendering uses a generic custom-surface contract.

A reusable primitive should make sense in at least three unrelated interfaces,
such as a HUD, inventory, and settings screen. Otherwise it should remain local
until a broader contract is demonstrated.

## Performance contract

The runtime is designed to avoid DOM, CSS cascade, general selector matching,
and stable-frame reconciliation.

Measure separately:

- Parse and compile time.
- Resident activation/deactivation.
- Stable update and render-list time.
- Value-only mutation.
- Layout-affecting mutation.
- Dense scroll and virtualized-grid behavior.
- Backend draw and complete host render time.
- Whole-process RSS.
- GLayout/GView-owned allocations and resident bytes.
- Binary size.
- Mean, p95, p99, and maximum timing.

Initial targets:

- Dense 1080p complete host UI below 3 ms.
- Comfortable 144 Hz headroom.
- Normal update/layout below 1 ms.
- No stable-frame allocation or layout work.
- Resident view activation within one 60/144 Hz frame.

The expectation that GView beats RmlUi is a hypothesis to prove. Reports must
not hide SDL, GPU-driver, font-atlas, or tool memory inside an ambiguous number.

## Code quality contract

- Keep source files around 300-500 lines or less.
- Split by cohesive ownership, not arbitrary size.
- Prefer mostly flat domain organization.
- Use direct, explicit, debugger-friendly C++.
- Put terse what-is comments above paragraph blocks.
- Avoid giant host/controller files and unrelated utility collections.
- Keep runtime, backend, tools, and trial domains separate.
- Preserve source diagnostics and test observable contracts.

## Existing assets to preserve

The Splonks-embedded Gubsy contains proven pieces that should be extracted or
generalized rather than blindly rewritten:

- Independent render/window resolution controls and grouped presets.
- Form-factor forcing and safe-area/scale/sampling preview controls.
- Live active-layout following and variant selection.
- Layout creation, duplication, generated IDs, multi-selection, snapping,
  drag/resize, undo/redo, and saving.
- Explicit directed widget navigation data and runtime behavior.

The current checked-out source contains hand-authored navigation edges but no
located visual focus-edge editor. Search repository history and related copies
before rebuilding it; if unavailable, implement it as a first-class GView tool.

## Repository integration

- Standalone GLayout is authoritative for layout code.
- Standalone GView is authoritative for presentation/runtime/tool code.
- Gubsy packages and adapts the standalone targets.
- The dirty Splonks worktrees are archaeological sources and must not be reset.
- Duplicated GLayout source inside Gubsy should be removed only after dependency
  integration is validated and unrelated behavior is preserved.

## Completion criteria

The first project is complete only when:

- GLayout remains independently useful and gains the required graph foundation.
- GView is a standalone documented repository with C++ and S-expression authoring.
- Gubsy consumes the libraries and exposes reusable authoring integration.
- The live layout and navigation authoring workflows operate on persisted assets.
- The complete reference trial is usable with mouse and controller.
- Visual inspection covers all main views and required target resolutions.
- Performance and memory meet or honestly characterize the target contract.
- Repositories pass tests, formatting, source-size, and clean-diff checks.
- Coherent commits are pushed and the trial is launched for user inspection.

## Explicit follow-on work

After this project is reviewed and iterated to satisfaction:

1. Build a suite of unrelated game UIs to pressure-test the abstractions.
2. Add only reusable missing capabilities.
3. Stabilize APIs and asset formats.
4. Update Splonks to the latest composed Gubsy.
5. Remove hardcoded Splonks menu implementation.
6. Build its production menus and game UI with GLayout/GView.

The multi-game suite and final Splonks migration are recorded here so they are
not forgotten, but they are not prerequisites for completing the first GView
implementation and trial.
