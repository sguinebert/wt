# UI2 Web Base Widgets Plan

Date: 2026-02-26
Status: In progress

## Scope

Design and ship the full baseline web widget set on top of `Wt::ui2` (hard-cut mode), with:

- static-shape compatibility,
- slot-driven O(k) patching,
- delegated event model,
- minimal runtime state on client/server bridge.

## Phase 0: Foundations (done/in-progress)

1. Contract lock:
- versioned contract (`ui2_contract_version`),
- explicit context contract fields (`MountContext`, `PatchContext`, `EventContext`).

2. Runtime core:
- `ShapeTree`, `SlotStore`, `PatchProgram`,
- `any_widget` type-erasure with SBO,
- `RuntimeInstance` lifecycle.

3. Basic content widgets:
- `RootWidget`, `ContainerWidget`, `SpanWidget`, `TextWidget`, `HtmlWidget`,
- in-memory `DomTree` representation and common prop patching.

## Phase 1: Interactive primitives (next)

1. Action and focus widgets:
- `ButtonWidget`, `LinkWidget`, `IconButtonWidget`.

2. Form input baseline:
- `LabelWidget`, `InputTextWidget`, `TextAreaWidget`,
- `CheckboxWidget`, `RadioWidget`, `SelectWidget`, `OptionWidget`, `FormWidget`.

3. Slot bindings:
- `value`, `checked`, `disabled`, `placeholder`, `name`, `type`, `href`, `target`.

4. Event bindings:
- `click`, `input`, `change`, `submit`, `focus`, `blur`,
- delegated event IDs and payload encoding.

## Phase 2: Structural/data primitives

1. List and table:
- `ListWidget` (`ul`/`ol`/`li`) with keyed item support,
- `TableWidget` (`table`/`thead`/`tbody`/`tr`/`td`/`th`).

2. Media:
- `ImageWidget`, `VideoWidget`, `AudioWidget`.

3. Progress/status:
- `BadgeWidget`, `ProgressWidget`, `MeterWidget`.

## Phase 3: Layout and overlay baseline

1. Layout containers:
- `FlexWidget`, `GridWidget`, `StackWidget`, `ScrollContainerWidget`, `SpacerWidget`.

2. Overlay/navigation:
- `DialogWidget`, `PopoverWidget`, `TooltipWidget`, `TabsWidget`, `AccordionWidget`.

3. Virtualized data:
- `VirtualListWidget`, `PaginatorWidget`.

## Cross-cutting requirements

1. Accessibility:
- role + aria attributes,
- keyboard navigation contracts,
- focus order consistency.

2. Performance:
- no full-tree walk on patch,
- dirty-slot-only operations,
- keyed list fast path before reorder fallback.

3. Determinism:
- stable node/slot IDs,
- deterministic mount/patch order,
- stable event routing IDs.

4. Testing gates:
- mount/unmount lifecycle,
- per-widget patch behavior,
- event payload correctness,
- regression perf scenarios (large tree, sparse slot updates).

## Exit criteria for "base web widgets complete"

1. All phase 1 + 2 widgets shipped and compiled in hard-cut mode.
2. Phase 3 `Dialog`, `Tabs`, `Flex`, `Grid`, `VirtualList` shipped.
3. CI lane with UI2-only test matrix green.
4. Legacy widget code path disabled in default build profile.
