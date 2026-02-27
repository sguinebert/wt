# RFC: Wt::Dbo Extreme C++26 Reflection Hard Cut

- Date: 2026-02-27
- Status: Proposed (replaces previous cutover plan)
- Scope: `src/Wt/Dbo` only

## Why This RFC Is Being Rewritten

The previous plan was still too incremental.
This RFC adopts an extreme position aligned with the reflection direction highlighted by Daniel Lemire:

1. Boilerplate mapping code is technical debt, not an API.
2. "Heavy lifting" must happen once at compile time, not repeatedly at runtime.
3. Reflection is not a cosmetic cleanup. It is a language-level architecture reset.

This is a hard cut. No legacy compatibility lane is kept inside mainline Dbo.

## Inputs

This redesign is based on:

1. Daniel Lemire, "C++26 will include compile-time reflection: why should you care?"
2. Herb Sutter, "Trip report: Summer ISO C++ standards meeting (St Louis, USA)"
3. WG21 P2996R13, static reflection core proposal (C++26 track)

## Non-Negotiable Principles

1. One mapping model only: C++26 reflection.
2. Zero `persist(Action&)` support in maintained code.
3. Zero `DbAction*` legacy layer.
4. Zero exception-based control flow in maintained Dbo code.
5. One error transport model: `std::expected`.
6. Compile-time metadata and SQL planning first; runtime only executes/binds.
7. Fail-fast toolchain gates at configure and compile time.

## Target End State

### API model

1. User model classes are plain C++ types.
2. Mapping is inferred from reflection metadata plus `dbo_meta<T>` customizations.
3. Legacy action visitors are removed.
4. Public APIs return `dbo_result<T>` (`std::expected<T, dbo_error>`) or `awaitable<dbo_result<T>>`.

### Runtime model

1. Runtime does not discover structure by dynamic visitor chains.
2. Runtime executes preplanned statement layouts generated from reflection descriptors.
3. Relation wiring (`belongs_to`, `has_many`, `has_one`, `many_to_many`) is handled by reflection iterators/actions only.

### Source tree model

Target layout for Dbo:

1. `src/Wt/Dbo/api/` - public ORM surface (`Session`, `Query`, `ptr`, `weak_ptr`, `collection`, `Transaction`)
2. `src/Wt/Dbo/meta/` - reflection metadata, iterators, schema/SQL planners
3. `src/Wt/Dbo/core/` - SQL runtime core, parsing, error domain, statement execution
4. `src/Wt/Dbo/backend/` - supported in-tree backends
5. `src/Wt/Dbo/compat/` - temporary migration shims (must be empty at final lot)

No long-lived mixed root folder after final lot.

## Removal Policy (Extreme)

The following is removed from maintained code, not deprecated:

1. `DbAction.h`, `DbAction_impl.h`, `DbAction.C`
2. `InitSchema`, `DropSchema` (legacy class forms)
3. `SaveDbAction`, `LoadDbAction`, `SetReciproceAction`, `DboAction` class hierarchy
4. `persist(Action&)` mapping path
5. Throw/catch based Dbo API contracts
6. Legacy exception-first docs/examples/tests

If a downstream project still uses these APIs, migration is required before upgrade.

## Error Model (Mandatory Hard Rule)

1. Canonical error payload: `dbo_error` (`code`, `message`, `backend`, `sqlstate`, `vendor_code`, `context`, optional table/id/version).
2. Canonical error codes: `enum class DboErrc`.
3. Sync APIs: `dbo_result<T>` / `dbo_result<void>`.
4. Async APIs: `awaitable<dbo_result<T>>`.
5. No `throw` and no `catch` in maintained Dbo code (`src/Wt/Dbo`, excluding vendored third-party subtrees if any).
6. CI hard gate enforces this rule.

## Compile-Time Reflection Strategy

This section is the architectural core.

1. Build a `consteval` reflection descriptor per mapped type.
2. Descriptor includes:
   1. table name
   2. id/version policy
   3. value fields (type, flags, size/default/no-mutation/aux-id/shard-id)
   4. relation fields (target type, join name/id, relation kind, fk flags)
3. Generate statement plans from descriptor:
   1. insert
   2. update
   3. select by id
   4. relation-select statements
4. Runtime uses generated plans only for bind/execute/read.
5. No runtime fallback to legacy visitor or stringly type inspection.

This is where "heavy lifting once" is applied concretely to Dbo internals.

## Query Path Strategy

1. Keep dynamic query strings for user-authored SQL where needed.
2. For ORM-generated statements, use reflection-generated layouts and bind order.
3. Keep parameter binding strongly typed through `SaveBaseAction` replacement (`BindAction` path) and reflection field traversal.
4. Remove hidden coupling from query internals to legacy action headers.

## Migration Lots (Reset Plan)

Previous lot numbering is superseded by this plan.

### Lot A: Hard Gates And Build Contract

Scope:

1. Enforce C++26 on `wtdbo`.
2. Enforce reflection capability probe (`<meta>` + reflection operator support).
3. Enforce compiler-specific reflection flags when required (for example GNU `-freflection`).
4. Add CI lane for reflection toolchain.
5. Add CI guard for no `throw`/`catch` rule.

Exit criteria:

1. `wtdbo` cannot configure without reflection support.
2. CI clearly reports unsupported toolchain failures.
3. Throw/catch budget in maintained Dbo code is zero.

### Lot B: Legacy Action Excision

Scope:

1. Remove all includes of `DbAction.h` / `DbAction_impl.h` from active code paths.
2. Replace remaining binder dependencies with `BindAction`.
3. Replace legacy drop-schema class dependency with reflection-native drop path.
4. Remove legacy friend declarations tied to removed action classes.
5. Delete `DbAction*` files from target and source tree.

Exit criteria:

1. `rg "DbAction|InitSchema|SaveDbAction|LoadDbAction|SetReciproceAction|DboAction" src/Wt/Dbo` returns no maintained-code hits.

### Lot C: Reflection Descriptor Kernel

Scope:

1. Introduce explicit type descriptor API in `meta/`.
2. Ensure schema and statement planning use the descriptor, not ad-hoc runtime state mutation.
3. Centralize relation classification and options extraction.
4. Add compile-time validation and diagnostics for unsupported member patterns.

Exit criteria:

1. `reflect_init_schema`, save/load, and relation setup all consume the same descriptor layer.
2. Duplicate field/relation classification logic is eliminated.

### Lot D: Exception-Free Surface Completion

Scope:

1. Finish conversion of all public Dbo APIs to `dbo_result`.
2. Remove remaining exception boundary adapters.
3. Remove obsolete exception classes from active API surface.
4. Normalize backend error conversion to `DboErrc`.

Exit criteria:

1. No maintained Dbo API documents exception throws.
2. Throw/catch grep gate remains zero.

### Lot E: Public API Hard Cut (`persist` Removal)

Scope:

1. Remove remaining documentation and comments describing `persist(Action&)` as mapping mechanism.
2. Remove code paths that require user-defined action visitors for mapping.
3. Keep only reflection metadata (`dbo_meta`) extension points.

Exit criteria:

1. `rg "persist\\s*\\(" src/Wt/Dbo examples test` has no mapping-path usage.

### Lot F: Filesystem Reorg

Scope:

1. Move files into `api/`, `meta/`, `core/`, `backend/`, `compat/`.
2. Update includes, install rules, exports.
3. Keep temporary forwarding headers only where unavoidable during transition.

Exit criteria:

1. Dbo root is no longer a flat mixed layer.
2. Forwarding headers are either removed or explicitly tracked for final deletion.

### Lot G: Backend Scope Rationalization

Scope:

1. Keep only actively maintained backends in-tree.
2. Define extension contract for out-of-tree backends.
3. Remove unowned backend code from core responsibility.

Exit criteria:

1. CI matrix matches declared supported backend set.

### Lot H: Tests And Examples Hard Cut

Scope:

1. Rewrite tests/examples to reflection metadata model.
2. Rewrite error assertions to `std::expected` outcomes.
3. Add dedicated reflection regression suite for schema, relations, query bind/load, and drop/init.

Exit criteria:

1. No legacy mapping or exception-first Dbo tests remain.
2. Reflection suite passes on supported compiler/backend matrix.

## Quality Gates (Always On)

1. `rg "\\bthrow\\b|\\bcatch\\b" src/Wt/Dbo --glob '!**/third_party/**'` must be empty.
2. `rg "DbAction|SaveDbAction|LoadDbAction|SetReciproceAction|InitSchema|DropSchema" src/Wt/Dbo` must be empty (except historical docs if explicitly allowed).
3. `rg "persist\\s*\\(" src/Wt/Dbo examples test` must be empty for mapping code.
4. `wtdbo` must build with reflection-required toolchain lane.
5. Reflection descriptor tests must pass before merge.

## Risk Position

This RFC deliberately accepts high breakage risk to remove architecture debt quickly.

Accepted tradeoff:

1. Short-term downstream pain
2. Medium-term maintenance simplicity
3. Long-term performance and correctness benefits from compile-time planning

Rejected tradeoff:

1. Multi-year dual-stack maintenance (legacy + reflection)

## PR Slicing Rules

1. One lot per PR.
2. No "mixed lot" PRs.
3. Each PR includes grep-proof evidence for its own exit criteria.
4. If a lot cannot close its compatibility shim, the PR does not merge.

## References

1. Daniel Lemire, C++26 reflection article:
   https://lemire.me/blog/2025/06/22/c26-will-include-compile-time-reflection-why-should-you-care/
2. Herb Sutter trip report:
   https://herbsutter.com/2024/06/26/trip-report-summer-iso-c-standards-meeting-st-louis-usa/
3. WG21 P2996R13:
   https://wg21.link/p2996r13
