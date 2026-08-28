## File Size

- Keep source files at roughly 300-500 lines or less.
- Split by cohesive ownership before a file becomes a mixed-domain controller.
- Small exceptions are acceptable when a split would obscure the code.

## Organization

- Organize modules around concrete domains and behavior.
- Keep the module tree mostly flat.
- Keep runtime, authoring tools, renderer backends, and trial content separate.
- Do not create vague utility files for unrelated helpers.
- Keep public data movement explicit and easy to inspect in a debugger.

## Style

- Prefer direct C/C+-style C++ over clever framework machinery.
- Prefer plain structs, handles, dense arrays, and explicit functions.
- Use early returns and shallow control flow.
- Put a terse comment above each paragraph block stating what it does.
- Comments explain ownership or intent; they do not narrate syntax.
- Do not perform speculative abstraction or application-specific generalization.

## Architecture

- GLayout owns geometry. GView owns presentation and interaction.
- Game code owns semantic content, actions, and custom rendering.
- ImGui and authoring code must remain optional and absent from release targets.
- Stable authored IDs may compile to dense runtime indices.
- Stable frames must not reconcile, lay out, or allocate without a dirty cause.
- S-expression and C++ authoring compile to the same validated representation.

## Quality

- A control is incomplete until mouse, keyboard, and controller behavior work.
- A screen is incomplete until all intended flows are reachable and visually inspected.
- New reusable primitives must apply naturally to multiple unrelated game interfaces.
- Keep trial-specific behavior in the trial rather than the library.
- Preserve explicit diagnostics for invalid layouts, bindings, and focus graphs.
