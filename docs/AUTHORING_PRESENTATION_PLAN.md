# GView authoring and presentation recovery plan

## Status and purpose

The authoring and presentation recovery implementation is complete as of
2026-08-28 and is ready for hands-on user review. This document remains the
durable specification and acceptance record. It records the workflow, defects,
ownership boundaries, implementation evidence, and user acceptance test so a
later iteration cannot silently narrow the goal.

The implementation makes the running native game UI the WYSIWYG editing
surface, recovers the useful parts of the earlier GLayout/Gubsy tools, adds the
missing semantic presentation model, and validates the standalone runtime and
the genuine Gubsy host boundary. The user-review gate is intentionally still
open; implementation completion is not the same as workflow acceptance.

Do not migrate Splonks to GView during this milestone. The authoring workflow
and reference trial must first be comfortable enough to build and tune a full
game interface without returning to hardcoded menu geometry.

## Implemented recovery record

The completed milestone provides:

- Explicit Test and Edit modes over the real native canvas, with clean, layout,
  focus, and combined overlays. Edit mode pauses ordinary menu input.
- Direct child selection, additive selection, eight resize handles, move,
  constraint-aware resize/reorder, grid and sibling snapping, guides, nudge,
  reparent, duplicate, cut/copy/paste, delete, undo/redo, atomic save, and reload.
- Safe staged focus authoring on the native canvas. Node overrides and group
  links are explicit Apply/Cancel operations rather than click-side effects.
- Generated local row/list/grid navigation plus group entry/exit, containment,
  Back policy, and exact remembered-member return without authored fake-data
  IDs such as `roster-moss`.
- All 36 recovered display presets, separate logical and physical sizes, device
  pixel ratio and user UI scale, safe area, form factor, fit/stretch/overscan/
  integer presentation, linear/nearest sampling, zoom, and pan.
- Compound slider/select/toggle presentation slots and asset-state recipes with
  S-expression and C++ authoring, inheritance, selectors, and compiled lookup.
- Density-aware glyph rasterization and corrected baseline, descender, wrapping,
  and scissor behavior. Popups, sliders, destination affordances, and toasts are
  now structurally bounded rather than repaired with per-screen offsets.
- A reusable `gubsy::ui::ViewRuntime` hosted through the normal Gubsy runtime,
  mapped input, SDL event flow, typed model/events, asset domains, and display
  resolution services. The standalone trial remains the complete visual and
  performance reference; the Gubsy engine-lifecycle smoke validates the host
  boundary without copying the 18-screen shell.
- A complete 18-state shell and inventory proof, a passing route/interaction
  self-test, and 72 inspected captures spanning all states at 1280x720,
  1920x1080, 960x540, and 720x1280.

Strict GView, GLayout, trial, Gubsy adapter, and consumer builds pass. The final
release dense-scroll path measures 0.2752 ms mean at 1280x720 and 0.3171 ms mean
at 1920x1080. GView-owned state is about 139 KiB for Play and 287 KiB for the
dense catalog. Exact timing boundaries and process RSS are recorded beside the
trial in `evaluation-notes/GVIEW_RESULTS.md`.

## Original review findings that must not be lost

The pre-recovery implementation demonstrated fast retained composition, a broad
reference shell, S-expression persistence, and optional tooling, but did not
yet demonstrate the intended production workflow.

The review found these blocking gaps; each is retained here as regression scope:

- Authoring is centered on detached ImGui representations instead of direct
  manipulation over the real native rendering.
- The detached focus graph is easy to mutate accidentally and obscures the
  spatial relationship between authored nodes and actual controls.
- The editor exposes insufficient focus-group, collection-template, and focus
  memory semantics.
- The simulator does not yet preserve the full legacy preset and preview tool.
- Widget rendering lacks explicit internal parts and asset-driven state skins.
- Select popups, sliders, links, overlays, and notifications still expose
  structural or interaction defects.
- The trial is standalone SDL3 code using GView/GLayout, not a complete proof
  hosted through Gubsy's normal loop, events, assets, input, and debug tools.
- Most trial text appears clipped at the bottom by roughly five percent. This
  is a cross-cutting typography/backend release blocker, not local polish.

Earlier benchmark results remain historical evidence only. The post-recovery
measurements and native workflow review are the acceptance evidence.

## Product outcome

A Gubsy game should register the standard UI tooling once and gain a live UI
workbench around its real renderer. An author should be able to:

1. Run the game with normal assets, text, animation, clipping, and compositing.
2. Pause UI/game navigation and enter an explicit authoring mode.
3. Select the real rendered controls directly on the game canvas.
4. Move, resize, constrain, reparent, duplicate, and style those controls.
5. Inspect and edit focus groups and directional relationships in place.
6. Switch internal resolution, output size, form factor, safe area, and scale.
7. Test mouse, keyboard, and controller behavior immediately.
8. Undo, redo, save normal source assets, reload, and continue editing.
9. Turn diagnostics off without leaving authoring mode to inspect final output.

The same authored representation must be available through S-expressions and a
C++ API. S-expressions are the canonical persisted source and hot-reload format;
the C++ builder targets the same validated model rather than a second system.

## Native-canvas authoring workflow

The native renderer is authoritative. Authoring overlays are drawn into the
same canvas as the actual interface so the author sees final assets, shaped
text, animation, clips, masks, custom surfaces, and game/world compositing.
ImGui supplies surrounding inspectors and commands; it does not replace the
canvas with a schematic miniature.

Entering authoring mode must suspend normal game/menu input. Pointer and
keyboard activity goes to the editor until the author explicitly returns to
test mode. This avoids accidentally activating controls or changing focus edges
while attempting to select geometry.

The canvas supports independent diagnostic modes:

- Clean preview while authoring remains active.
- Layout bounds, IDs, hierarchy colors, anchors, and handles.
- Grid, ruler, safe-area, baseline, and snapping guides.
- Focus nodes, groups, current focus, remembered members, and directed edges.
- Combined layout and navigation overlays.
- Selected-node isolation to reduce clutter.

The detached graph overview may remain as an optional diagnostic for large
graphs. It is not the primary editing surface and must never be the only way to
understand or edit a native control.

ImGui utilities should be separate, focused windows: display simulation,
selection/properties, hierarchy, focus relationships, theme/assets, runtime
telemetry, and diagnostics. Do not mash all controls into one oversized panel.

## Direct layout editing

The editor must preserve and improve the mature flat-rectangle workflow that
already exists in GLayout and its Gubsy integration:

- Direct native-node selection and additive multi-selection.
- Drag from the center to move.
- Drag all four edges and four corners to resize.
- Adjustable visible grid and grid snapping.
- Sibling edge and center snapping with temporary guides.
- Group movement and resizing.
- Arrow-key nudging with coarse/fine modifiers.
- Cut, copy, paste, duplicate, and delete.
- Create and reparent nodes.
- Undo and redo every persisted mutation.
- Numeric geometry and property editing.
- Atomic save, reload, and restore.

The implementation must inspect and preserve useful behavior from:

- `glayout/include/glayout/editor.hpp`
- `glayout/src/editor.cpp`
- `glayout/src/editor_overlay.cpp`
- `glayout/examples/sdl_demo`
- `gubsy/src/layout_editor`
- `gubsy/docs/ui_layout_editor_plan.md`

The old implementation is archaeological evidence, not a required internal
design. Preserve its capabilities and feel while replacing weak internals.

Direct manipulation must respect the active layout model instead of silently
flattening everything to absolute rectangles:

- Absolute and overlay children edit their resolved rectangle or offsets.
- Row, column, and grid children edit size, order, tracks, gaps, and alignment.
- Anchored children edit anchors and anchor offsets.
- Fixed children edit dimensions.
- Responsive edits apply to the explicit active variant.
- Ambiguous edits present a clear choice rather than corrupting constraints.

GLayout owns these renderer-free geometry operations. GView identifies the
semantic node and appropriate internal presentation part. Gubsy hosts input,
preview, persistence, and debug-window registration.

## Focus and navigation authoring

Navigation is a semantic graph over controls, collections, and scopes. It is
not controller-as-mouse and it is not merely a set of nearest-neighbor node IDs.

The in-canvas workflow is:

1. Select a native source control or group.
2. Arm Up, Down, Left, Right, Back, tab, bumper, or another semantic action.
3. Select the native destination control or group.
4. Confirm the relationship explicitly.
5. Test it immediately using the controller.
6. Remove, replace, undo, redo, and save it.

Arming, confirmation, and cancellation must be visually unmistakable. Merely
clicking around a graph must not silently rewrite bindings.

The persisted model must support:

- Nested focus scopes and visible group boundaries.
- Generated row, column, grid, and list relationships.
- Explicit exceptional edges.
- Group-level entry and exit relationships.
- Remembered focus per group and per view.
- Entry policies such as remembered, first, last, fixed, nearest, and
  corresponding row or index when meaningful.
- Modal containment and local Back behavior.
- Tab and bumper transitions.
- Disabled, hidden, removed, and variant-specific targets.

A common acceptance example is a horizontal collection above a button. Down
from any item enters the button; Up from the button returns to the exact item
previously left. The graph should persist group policy and memory rather than a
separate pair of authored edges for every collection item.

Dynamic content identities such as `roster-moss` and `roster-vega` must not be
hardcoded authored graph nodes. The author targets a collection, template, or
stable semantic key policy. The runtime may expose materialized instances for
inspection while persistence remains independent of current fake data.

Diagnostics must report unreachable controls, dead ends, trapped scopes,
invalid modal exits, missing destinations, and references absent in an active
responsive variant. Cycles are allowed when they encode intentional wrapping
or return paths.

The earlier native navigation editor should be studied in Gubsy history at
commit `fcf9106`, especially `src/main_menu/menu_navgraph.cpp`; its deletion at
`c7140bb` helps locate the surrounding history. Recover its useful native-canvas
workflow without preserving its primitive node-only limitations.

## Display and device simulation

The existing Gubsy video tool is the authoritative starting point:
`gubsy/src/imgui_debug/video_window.cpp`. It separates render resolution and
window size and includes custom dimensions, matching, display modes, fit and
stretch, nearest and linear sampling, zoom, pan, safe areas, form-factor forcing,
and reset operations.

Preserve the complete preset catalog rather than replacing it with a short list:

- 16:9: 1280x720, 1366x768, 1920x1080, 2560x1440, 3840x2160, 7680x4320.
- 16:10: 1280x800, 1440x900, 1680x1050, 1920x1200, 2560x1600, 2880x1800.
- 21:9: 2560x1080, 3440x1440, 5120x2160.
- 32:9: 3840x1080, 5120x1440.
- Console and retro: 1280x720, 640x480, 480x272, 480x270, 320x240,
  256x192, and 240x160.
- Phone/handheld portrait: 160x144, 720x1280, 1080x1920, 1080x2340,
  1080x2400, 1170x2532, 1179x2556, and 1440x3200.
- Tablet portrait: 1536x2048, 1668x2388, 2048x2732, and 1600x2560.

Modern mobile presets require more than a physical framebuffer. Each preset
should carry or derive:

- Logical viewport dimensions.
- Physical framebuffer dimensions.
- Device pixel scale.
- Orientation and form-factor bias.
- Safe-area insets.
- A separately adjustable UI/accessibility scale.

For example, a 1179x2556 iPhone framebuffer commonly corresponds to a 393x852
logical viewport at 3x. Exact modern device metadata must be checked against
authoritative sources during implementation. Device pixel scale and user UI
scale are separate concepts and must not be collapsed into one DPI slider.

Switching presets must update the native preview, active responsive variant,
safe area, text rasterization, clips, overlays, and diagnostics immediately.

## Semantic controls and presentation recipes

GView controls are semantic behavior with compound visual parts. They are not
hardcoded green rectangles, and internal decoration is never a focus target.

Required compound anatomy includes:

- Slider: label, description, value, track, fill, thumb, and optional ticks.
- Select: label, current value, open indicator, popup frame, and option template.
- Toggle: on/off background, knob, state marks, and transitions.
- Button and destination link: content plus distinct action/navigation affordance.
- Tabs, panels, group backgrounds, scrollbars, modals, toasts, tooltips, and
  controller prompt icons.

Each part may have normal, focused, hovered, pressed, disabled, selected, open,
on, and off presentation. Supported visual operations should include natural
size, stretch, tile, nine-slice, contain, cover, crop, tint, opacity, atlas
regions, gradients, and animated frames. Shader/material hooks may be added
later through a generic backend contract when a real interface requires them.

Theme resolution follows this precedence:

1. Gubsy fallback presentation.
2. Game theme.
3. Screen theme.
4. Widget class.
5. Specific widget.
6. Runtime state.

Theme definitions belong in the shared S-expression model and have an
equivalent C++ builder. A representative source shape is:

```lisp
(theme
  (id "splonks")
  (extends "gubsy-default")
  (widget (kind slider)
    (part (id track) (asset "game:ui/rope_track") (image-mode tile))
    (part (id fill) (asset "game:ui/rope_lit") (image-mode tile))
    (part (id knob) (asset "game:ui/climbing_pin"))
    (part (id knob) (state focused)
      (asset "game:ui/climbing_pin_glow")))
  (screen (id "quest-picker")
    (override (widget-kind slider) (part track)
      (asset "game:ui/map_path"))))
```

Parsing, inheritance, and validation occur on load or hot reload. Runtime theme
lookup compiles to dense resolved indices and cached opaque asset handles; it
must not repeatedly interpret strings or traverse an inheritance tree per frame.

This asset-driven model must support interfaces as visually different as an
SSX card rack, a handwritten notebook, a Zelda inventory, a conventional
settings panel, and a HUD without creating game-specific library widgets.

## Immediate widget and rendering defects

These defects block acceptance of the current trial and must receive explicit
regression coverage.

### Text baseline and clipping

Most current trial text loses approximately the bottom five percent. Diagnose
the shared text path using true ascender, descender, baseline, line-gap, glyph
surface bounds, framebuffer scale, and scissor rectangles. Do not mask the bug
with arbitrary padding in each widget.

Visual tests must include `g`, `j`, `p`, `q`, and `y`, mixed capitals, wrapped
lines, multiple font sizes, and every representative logical/physical scale.
Baseline and clip overlays should make metric disagreement directly visible.

### Slider composition

Slider labels, values, tracks, fills, and thumbs currently occupy overlapping
geometry. Replace percentage offsets inside one rectangle with explicit
compound slots resolved by GLayout. The track, fill, and thumb must align on
the same axis and remain usable with mouse, keyboard, and controller.

### Select and popup behavior

Closed selects need a clear open indicator. Open selects need a real portal or
popup frame with padding, opaque/elevated background, selected state, clipping,
scrolling, viewport-aware placement, and explicit commit/cancel behavior. An
open option must not be mashed against the left edge or inherit accidental
transparency from the underlying panel.

### Buttons and destinations

Buttons, inline actions, and links to nested views need distinguishable
affordances. Interaction semantics should not rely on the author remembering
that every generic outlined rectangle behaves differently.

### Toast and overlay bounds

The `Player added` feedback is intended as a small mock-state toast. It
currently sometimes resolves as a full-screen green overlay. Fix overlay
coordinate-space, strata, clipping, and dismissal behavior. A local toast must
never unexpectedly obscure the entire application.

### Visual hierarchy

Restore useful reference assets, gradients, panel treatments, and background
layers from the Vue/RmlUi design evidence. Exact colors are not the goal, but
screens must have legible grouping and hierarchy rather than one undifferentiated
flat field.

## Ownership boundaries

GLayout owns geometry only: hierarchy, rows, columns, grids, overlays, absolute
placement, sizing, constraints, padding, gaps, alignment, anchors, clips,
scroll extents, responsive variants, resolved rectangles, and generic direct
manipulation/snapping operations. It does not know controls or assets.

GView owns semantic presentation and interaction: controls, focus, collections,
scroll state, compound widget parts, stateful theme resolution, presentation
animation, overlays, paint intent, and opaque asset references. It expands a
semantic widget into internal GLayout parts without exposing those parts to
focus navigation.

Renderer backends own texture drawing, tiling, nine-slice, atlas access,
gradients, clipping/masks, tint/opacity, font rasterization, and eventual
material hooks.

Gubsy owns engine hosting: asset domains and lifetime, hot reload, semantic
events and mapped input, controllers and local players, logical/physical output
metadata, safe areas, common device presets, debug-tool registration, theme/mod
layers, and game-world compositing.

Game code owns art, semantic content, gameplay policy, per-game themes,
screen/widget overrides, and unusual custom surfaces.

Do not create another generic paint library during this milestone. Extract one
later only if several non-GView consumers prove the boundary.

## Standalone and Gubsy-hosted validation

The standalone GView trial remains the clean performance and portability proof.
It must continue to build without Gubsy and remain useful for profiling core
layout, composition, text, and backend costs.

A genuine host path runs through Gubsy's normal runtime lifecycle and uses
Gubsy events, mapped input, controller/device ownership, asset domains, and
logical/physical display services through `gubsy::ui::ViewRuntime`. It validates
the reusable host boundary and normal engine services rather than a struct-only
adapter. The standalone executable remains the sole complete visual shell so
the same content is not copied into a second presentation application.

Both paths should consume the same authored view and theme definitions where
their host policies permit it. Differences must be deliberate adapter code,
not copied screens or divergent source assets.

The reference implementation remains the full Vue/RmlUi shell content:
Play and its nested flows, Players, Settings, Controls, Progress, Mods, and the
non-menu inventory/custom-surface proof. It requires real assets, compound
controls, nested state, scrolling, overlays, mouse, keyboard, controller, and
responsive layouts at 1280x720, 1920x1080, and representative compact/mobile
logical and physical configurations.

## Performance and release separation

Authoring code remains optional and absent from release targets. Measure the
release runtime separately from the authoring host.

Preserve the existing performance contract:

- Dense 1920x1080 complete host UI below 3 ms on the reference machine.
- Comfortable 144 Hz headroom.
- Ordinary update/layout below 1 ms.
- No stable-frame allocation, reconciliation, or layout work.
- Activation and deactivation within one 60/144 Hz frame.

Report parse/compile, activation, stable update, value mutation, layout mutation,
dense scrolling, text, backend recording, full host render, allocations,
library-owned resident bytes, whole-process RSS, and binary size. Do not hide
SDL, driver, font atlas, or authoring-tool memory inside an ambiguous total.

## Code and repository discipline

Follow the Adventures-with-Chickens/Gubsy style throughout:

- Cohesive behavior and ownership domains.
- Roughly 300-500 lines maximum per source file.
- Mostly flat, easy-to-navigate module trees.
- Direct, low-magic, debugger-friendly C++.
- Terse what-is comments above paragraph blocks.
- No mixed-domain host/controller monoliths.
- No trial-specific meaning promoted into reusable libraries.

Standalone GLayout and GView remain authoritative. Gubsy composes their public
targets and owns adapters. The shared trials remain comparison and integration
evidence rather than a source-code dumping ground.

## Completed implementation sequence

The recovery proceeded in this order, revising details only when direct
evidence required it:

1. Reproduce and diagnose text baseline/clipping, toast bounds, slider overlap,
   select popup defects, and current authoring input hazards.
2. Launch and inspect the legacy GLayout/Gubsy editors and recover their exact
   useful operations and preset behavior.
3. Define compound widget slots, theme inheritance, asset parts, and compiled
   presentation data in GView/S-expression/C++ APIs.
4. Make native-canvas layout selection and manipulation the primary editor.
5. Add group-aware in-canvas focus authoring, memory policies, diagnostics, and
   safe edit/test modes.
6. Generalize the complete display simulator with logical/physical mobile data,
   device scale, UI scale, safe area, orientation, presets, zoom, and pan.
7. Repair and polish all reference controls using the generic primitives,
   including hierarchy assets and gradients.
8. Add the genuinely Gubsy-hosted reference path using normal engine services.
9. Run complete visual, interaction, persistence, controller, memory, startup,
   and performance validation across standalone and hosted paths.
10. Present the running trial for review; do not declare the milestone accepted
    or begin Splonks migration until the user passes the workflow.

## User acceptance gate

Implementation is complete, but user acceptance remains open. This milestone
passes workflow review only when the user can build and tune the reference UI on
the native canvas, switch the recovered display presets, manipulate constrained
layouts safely, edit and test group-aware controller traversal, skin compound
widgets with game assets, save/reload ordinary sources, and inspect a clean
render without leaving authoring mode.

All main and nested screens must be reachable and usable. Text must retain its
descenders, popups and overlays must be correctly bounded, controls must expose
clear affordances, dynamic collections must not leak fake-data IDs into authored
graphs, and the same definitions must work in standalone and real Gubsy-hosted
trials within the measured performance contract.
