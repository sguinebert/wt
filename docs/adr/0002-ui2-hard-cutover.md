# ADR-0002: UI2 Hard Cutover (Legacy WWidget Stack Disabled)

- Date: 2026-02-26
- Status: Accepted (in-progress)

## Context

The legacy widget stack is based on a large virtual interface (`WWidget`) with many dynamic dispatch points.
This model is not aligned with the UI2 goals:

- static UI shape as source of truth,
- minimal runtime state (`slots + refs + dirty bits`),
- O(k) patching over changed slots,
- concept-based extensibility without inheritance constraints.

## Decision

We enable a hard cutover mode (`WT_UI2_HARD_CUT=ON`) that:

1. builds `wt-widgets` from UI2 runtime only,
2. skips compiling the legacy WWidget-based widget stack,
3. skips legacy examples/tests/wttest that depend on old widgets.

UI2 core currently includes:

- `Wt/ui2/widget_like.hpp` (concept contracts),
- `Wt/ui2/any_widget.hpp` (type erasure + SBO),
- `Wt/ui2/shape_tree.hpp` (static tree model),
- `Wt/ui2/slot_store.hpp` (minimal dynamic state + dirty tracking),
- `Wt/ui2/patch_vm.hpp` (slot-indexed patch execution),
- `Wt/ui2/runtime.*` (instance lifecycle).

## Consequences

Positive:

- immediate break from virtual-heavy legacy path,
- clear performance-oriented architecture boundary,
- migration pressure to UI2 contracts.

Tradeoffs:

- examples/tests using legacy widgets are disabled in hard-cut mode,
- API compatibility with old widget stack is intentionally broken.

## Follow-Up

1. Add concrete UI2 primitive widgets (container/text/button/input/list).
2. Connect QML static codegen output directly to UI2 `ShapeTree + PatchProgram`.
3. Define binary patch protocol and action/event bridge.
4. Re-enable tests/examples on UI2-native fixtures.
