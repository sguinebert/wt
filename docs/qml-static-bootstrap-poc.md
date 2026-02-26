# QML Static Bootstrap PoC

This PoC demonstrates a modern bootstrap contract:

- QML compiles to a static **typed C++ widget tree**,
- server instantiates widgets directly from this C++ tree,
- client runtime remains TypeScript-built ESM for interaction wiring (`data-wt-action`),
- runtime artifact is content-hashed and tracked in a manifest.

## Files

- C++ static tree + instantiation helper:
  - `src/Wt/cpp26/qml_static_bootstrap_poc.hpp`
- QML-like to C++ codegen script:
  - `tools/qml_static_codegen.py`
- QML-like sample input:
  - `docs/qml-static-sample.qml`
- Generated sample output:
  - `src/Wt/cpp26/generated/sample_compiled_tree.hpp`
- TS runtime source:
  - `ts/src/qml-static/bootstrap.ts`
- TS build script (hash + manifest):
  - `ts/scripts/build-static-runtime.mjs`
- ADR:
  - `docs/adr/0001-stateless-session-static-ui-tree.md`

## Build Runtime

```bash
cd ts
npm run build:static-runtime
```

Outputs:

- `ts/public/js/static/qml-static-runtime.<hash>.mjs`
- `ts/public/js/static/qml-static-manifest.json`

## C++ Static Tree PoC

The helper defines:

1. `StaticWidgetNode` as compile-time tree nodes (`WidgetKind`, `WidgetProps`, children),
2. `instantiate_widget_tree(...)` to build real `WWidget` objects,
3. runtime bootstrap helpers:
   - `build_runtime_config_script_tag(...)`
   - `build_runtime_loader_script_tag(...)`
   - `build_runtime_bootstrap_html(...)`
4. `sample_compiled_tree` as an example of generated C++ output from QML.

The important point is that tree construction is C++-native, not JSON-driven.

## Codegen (QML-like -> C++)

Generate a header from the sample QML-like file:

```bash
python3 tools/qml_static_codegen.py \
  docs/qml-static-sample.qml \
  src/Wt/cpp26/generated/sample_compiled_tree.hpp \
  --symbol sample_compiled_tree_from_qml
```

Supported node types:

- `Container`
- `Text`
- `PushButton`

Supported properties:

- `id`
- `class`
- `text`
- `action`

## Runtime side

The runtime no longer constructs the tree.  
It only:

1. reads optional runtime config from `#wt-qml-static-config`,
2. installs a click bridge on `[data-wt-action]`,
3. posts actions to the configured endpoint.

Server-side HTML integration can directly reuse:

```cpp
auto runtime = Wt::cpp26::qml_static::build_runtime_bootstrap_html();
// Insert `runtime` in your page template/footer.
```
