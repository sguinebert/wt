# wtfmtdbo3 — Wt fork, radical C++26 modernization

> **Breaking changes are intentional and relentless.**
> This is not a drop-in replacement for Wt. It is a full architectural rethink built on C++26.

---

## Vision

The goal is a **declarative, QML-inspired description of web pages** that compiles to pure C++26, producing a single codebase that can be deployed in two modes simultaneously:

- **Server-side**: renders HTML/JS on the server, streams to the browser
- **WebAssembly**: the same C++ compiled to WASM, runs in the browser with no server round-trip

The compiler pipeline:

```
QML-like DSL
     │
     ▼
 C++ codegen  ──────────────────────────────────────────┐
     │                                                   │
     ▼                                                   ▼
Server binary                                  Emscripten / Wasm
(libwthttp → HTML/JS delivery)                 (runs in browser)
```

---

## Architecture overview

### 1. Widget layer — C++26 reflection

`src/Wt/cpp26/reflection.hpp`

C++26 `<meta>` reflection is the cornerstone of the new widget system.

- **Compile-time widget discovery**: `get_all_derived_widgets<ns, Base>()` walks every namespace at compile time, collects all classes derived from `WWidget`, sorted by inheritance depth (most-derived first).
- **Dynamic dispatch without vtables**: `dispatch(widget, fn)` uses `template for` + `dynamic_cast` to invoke a strongly-typed lambda on the actual concrete type.
- **Automatic JSON serialization**: `make_object_for<T>()` builds a Glaze object descriptor from reflected data members — no manual `glz::meta` specializations needed. Any `WWidget` subclass is serializable for free.
- **Factory deserialization**: `read_with_header_factory<Base>(blob)` parses a tagged wire format (`"TypeName\n{...json...}"`) and reconstructs the correct concrete type.

### 2. HTTP stack — two libraries

| Library | Protocol | Form | Dependencies |
|---------|----------|------|-------------|
| `libwt` | HTTP/1.1 | **Header-only** (cuehttp `connection.hpp`, `server.hpp`) | Boost.Asio |
| `libwhttp` | HTTP/2 | Compiled (`h2_flush_impl.C`, `h2_mixin.hpp`, `h2_connection.hpp`) | nghttp2 (bundled static) |
| `libwhttp` | HTTP/3 | Compiled (`h3_server.C`, `h3_stream_adapter.hpp`, `nexus/`) | lsquic + BoringSSL (bundled static) |

**Dependency graph:**
```
libwhttp  (standalone HTTP/2+3 server)
    ↑
  libwt   (widget framework, links whttp)
```

- `libwhttp` is **standalone** — link it alone for an HTTP/2+3 server without the widget framework.
- `libwt` links `libwhttp` publicly — linking `wt` gives you everything.
- HTTP/1.1 code is **header-only** in `src/Wt/cuehttp/` — no `.C` files, no library to link.
- HTTP/2 uses a **CRTP mixin** (`h2_mixin.hpp`) injected into `h2_connection.hpp`.
- HTTP/3 uses a **standalone `h3_server`** class (UDP-based, wraps nexus/lsquic). Can't share the TCP `server.hpp` template. Implementation in `h3_server.C` (pimpl), compiled with `BORINGSSL_PREFIX=BSSL` to avoid symbol conflicts with system OpenSSL.
- `server.hpp` is parameterized: `server<Socket, Connection>` — defaults to HTTP/1.1, use `h2_connection` for HTTP/2.
- All three protocols share the **same handler signature**: `std::function<awaitable<void>(context&)>`.
- **All protocol dependencies are statically embedded** in `libwhttp.so`: lsquic, BoringSSL (BSSL_-prefixed), nghttp2. Users only need system OpenSSL (for HTTP/1.1+2 TLS), Boost, and libc.

The async model is **pure coroutines**: every I/O path uses `co_await` on Asio awaitables. No callbacks, no raw threads managing lifetime.

### 3. DBO (ORM) — async + reflection

`src/Wt/Dbo/`

- All connection operations are `co_await`-able.
- Variant-based polymorphism is removed; unified connection interface replaces it.
- C++26 reflection will drive schema introspection (no more manual `persist()` specializations — WIP).
- Supported backends: PostgreSQL, SQLite3, MySQL/MariaDB, MS SQL Server, Firebird.

### 4. JSON

Three libraries are in-tree or evaluated:

| Library | Status | Role |
|---------|--------|------|
| **Glaze** | **Primary** — fully integrated | Widget serialization, reflection-driven, BEVE binary format, JMESPath, CSV |
| **boost::json** | Already in place | Legacy paths, HTTP body parsing |
| **simdjson** | Under evaluation | High-throughput ingestion (DOM/On-Demand) |

Decision criteria: Glaze wins for reflection-coupled serialization. simdjson is a candidate for raw ingestion performance. boost::json stays where Boost is already a dependency.

### 5. Build system

```
cmake -DBUILD_WASM=ON -DCMAKE_CXX_STANDARD=26 ..
```

- `BUILD_WASM` (default ON) enables the Emscripten client-side target.
- `cmake/WtFindBoost.cmake` replaces the old `WtFindBoost.txt`.
- CLI argument parsing: CLI11 v2.6.1 (`src/Wt/CLI11.hpp`, single header, BSD).

### 6. Static bootstrap PoC (QML tree)

An experimental stateless bootstrap flow exists for the upcoming QML pipeline:

- ADR: `docs/adr/0001-stateless-session-static-ui-tree.md`
- C++ static tree + instantiation helper: `src/Wt/cpp26/qml_static_bootstrap_poc.hpp`
- QML-like to C++ codegen: `tools/qml_static_codegen.py`
- Sample input/output: `docs/qml-static-sample.qml` -> `src/Wt/cpp26/generated/sample_compiled_tree.hpp`
- TS action-bridge runtime: `ts/src/qml-static/bootstrap.ts`
- Hashed runtime build + manifest: `ts/scripts/build-static-runtime.mjs`

Runtime output:

- `ts/public/js/static/qml-static-runtime.<hash>.mjs`
- `ts/public/js/static/qml-static-manifest.json`

---

## Dependency matrix

| Dependency | Required | Notes |
|------------|----------|-------|
| CMake ≥ 3.25 | yes | C++26 module support |
| Clang ≥ 21 (P2996 branch) | yes | C++26 `<meta>` reflection |
| Boost ≥ 1.85 | yes | Asio, beast, json (reducing scope over time) |
| Emscripten | for WASM | client-side target |
| OpenSSL | optional | TLS for HTTP/1.1+2 (system libssl/libcrypto) |
| nghttp2 | **bundled** | `src/3rdparty/nghttp2-src/` — static PIC, embedded in `libwhttp.so` |
| lsquic + BoringSSL | **bundled** | `src/3rdparty/lsquic-src/`, `src/3rdparty/boringssl-src/` — merged into `liblsquic_bundle.a`, BoringSSL symbols prefixed with `BSSL_` to coexist with system OpenSSL |
| Glaze | bundled | `src/glaze/` — header-only |
| CLI11 | bundled | `src/Wt/CLI11.hpp` — header-only |

---

## What is intentionally broken vs. upstream Wt

- `WApplication` session model: being redesigned for stateless server + stateful WASM.
- `persist()` DBO specialization: will be replaced by reflection.
- FastCGI / ISAPI connectors: not a priority, likely removed.
- `WEnvironment` / URL-based session routing: redesigned.
- All signal/slot wiring that crosses the server↔client boundary: replaced by the QML→C++ compilation pipeline.

---

## Repository structure

```
src/
├── Wt/
│   ├── cpp26/          # C++26 reflection utilities
│   ├── cuehttp/        # HTTP server (header-only 1.1 in libwt, 2+3 in libwhttp)
│   │   ├── detail/
│   │   │   ├── connection.hpp      # HTTP/1.1 base connection (header-only)
│   │   │   ├── h2_mixin.hpp        # CRTP mixin — all nghttp2 logic (libwhttp)
│   │   │   ├── h2_connection.hpp   # HTTP/2 connection = base + mixin (libwhttp)
│   │   │   ├── h2_flush_impl.C    # HTTP/2 flush (libwhttp compiled unit)
│   │   │   └── h3_stream_adapter.hpp  # Bridges nexus h3 stream → context
│   │   ├── nexus/                  # HTTP/3 transport (lsquic, libwhttp)
│   │   ├── h3_server.hpp           # HTTP/3 server declaration (pimpl)
│   │   ├── h3_server.C             # HTTP/3 server impl (compiled with BSSL prefix)
│   │   ├── server.hpp              # Parameterized server<Socket, Connection>
│   │   └── server_h2.hpp           # Convenience alias for HTTP/2 server
│   ├── Dbo/            # ORM (async rewrite in progress)
│   ├── CLI11.hpp       # CLI argument parsing (bundled)
│   └── ...             # Widget classes (being refactored)
├── glaze/              # Glaze JSON library (bundled, header-only)
├── 3rdparty/
│   ├── nghttp2-src/    # nghttp2 v1.64.0 (static PIC build)
│   ├── lsquic-src/     # lsquic (static, merged bundle with BoringSSL)
│   └── boringssl-src/  # BoringSSL (BSSL_ prefix, PIC)
Wasm/                   # WebAssembly client build (Emscripten)
ts/                     # TypeScript sources for JS widget bridges
docs/adr/               # Architecture decision records
cmake/
├── WtFindBoost.cmake
├── WtFindLsQuic.txt    # Finds lsquic bundle + BoringSSL prefix headers
└── WtFindNghttp2.cmake # Finds nghttp2 (prefers 3rdparty static)
```

---

## Status

| Subsystem | State |
|-----------|-------|
| C++26 reflection core | Experimental — compiles on Clang P2996 branch |
| Glaze integration | Functional — auto-serialization of `WWidget` subclasses |
| HTTP/1.1 coroutine server | Working — tested with curl |
| HTTP/2 server | Working — tested with `curl --http2` |
| HTTP/3 server | Working — UDP listener active, needs curl with HTTP/3 for e2e test |
| Static dependencies | lsquic + BoringSSL + nghttp2 all embedded in `libwhttp.so` |
| DBO async rewrite | Largely done, variant removal complete |
| WASM build | CMake scaffolding in place, TS loader ready |
| Static QML bootstrap PoC | Implemented (ADR + C++ helper + TS hashed runtime) |
| QML compiler | Not started |

---

## License

Original Wt code: GPL-2.0 with OpenSSL exception (or commercial — see `LICENSE`).
New code in this fork: same GPL-2.0 unless otherwise noted in file headers.
