# agents.md — Guide for AI agents working on this repo

This document tells every agent (Claude, Copilot, Cursor, etc.) what this codebase is, what the rules are, and how to work on it correctly. **Read this before touching any file.**

---

## What this project is

A radical fork of [Wt 4.x](https://www.webtoolkit.eu/wt) rewritten for C++26. The end goal is a **QML-like declarative language** for web pages that compiles to C++, deployable as both a server-side renderer and a WebAssembly client. Every subsystem is being rewritten with this target in mind.

**Breaking changes are the norm, not the exception.**

---

## Core rules for agents

### 1. C++ standard is C++26

- Use `<meta>` reflection, `template for`, `^^`, `[:…:]` splices freely.
- The compiler is the **Clang P2996 branch** (or any clang ≥ 21 with `std::meta`).
- `consteval`, `constexpr if`, `std::define_static_array` — use them.
- Do **not** add `#ifdef` guards for older standards unless explicitly asked.

### 2. Async model: coroutines everywhere

- All I/O must use `co_await` on Asio awaitables (`asio::awaitable<T>`).
- No raw callbacks, no `std::future`, no `std::thread` for I/O.
- DBO operations: every database call is `co_await`-able. Do not add sync wrappers.

### 3. Breaking changes are welcome

- Do not add backwards-compat shims.
- Do not preserve the upstream Wt API surface unless a feature is explicitly kept.
- If removing a function breaks something, fix the call site — do not keep the old function.
- Do not add deprecated attributes; just delete the old code.

### 4. JSON: use Glaze for new code

- New serialization code uses **Glaze** (`src/glaze/`).
- `boost::json` stays only where it already exists and there is no benefit to migrating.
- **simdjson** is under evaluation for high-throughput parsing — do not add it without discussion.
- Never add a third JSON library without updating this file and `README.md`.

### 5. HTTP stack: cuehttp — two libraries

- `src/Wt/cuehttp/` is the HTTP server, split into two CMake targets:
  - **`libwt`**: HTTP/1.1 only, header-only (`connection.hpp`, `server.hpp`, `response.hpp`)
  - **`libwhttp`**: HTTP/2 (`h2_flush_impl.C`, `h2_mixin.hpp`, `h2_connection.hpp`) + HTTP/3 (`nexus/`)
- `libwhttp` is **standalone** — it does NOT depend on `libwt`. `libwt` links `libwhttp`.
- HTTP/1.1 headers must have **zero** `#include <nghttp2/*>` or lsquic includes.
- HTTP/2 logic lives in a **CRTP mixin** (`h2_mixin.hpp`), composed into `h2_connection.hpp`.
- `server.hpp` accepts a `Connection` template parameter (default: `connection` = HTTP/1.1).
- `response.hpp` uses a type-erased `h2_flush_fn_t` callback — no dependency on `stream.hpp`.
- HTTP/3 uses a **standalone `h3_server`** class (`h3_server.hpp` + `h3_server.C`) — UDP-based, wraps `nexus::h3`.
- `h3_server.C` uses **pimpl pattern**: all `SSL_CTX` operations happen in the `.C` file, compiled with `BORINGSSL_PREFIX=BSSL`. The header has no SSL dependencies.
- HTTP/3 integration: `h3_stream_adapter.hpp` bridges `nexus::h3::stream` → `context` (same pseudo-header mapping as HTTP/2).
- All three protocols share the same handler: `std::function<awaitable<void>(context&)>`.
- **Static embedding**: lsquic, BoringSSL, and nghttp2 are all statically linked into `libwhttp.so`. Users do not need to install them.

### 6. No new Boost dependencies

- Boost scope is being **reduced**: Asio, Beast, json are kept for now.
- `boost::program_options` is being replaced by CLI11 (`src/Wt/CLI11.hpp`).
- Do not add new `#include <boost/…>` headers. If you need something Boost provides, find a C++26 stdlib or bundled alternative first.

### 7. DBO: reflect, don't specialize

- The `persist()` template specialization pattern is being replaced by C++26 reflection.
- Do not add new `persist()` specializations.
- Prefer `nonstatic_data_members_of` + `glz::object` for new mapping code.

### 8. QML static bootstrap runtime (PoC)

- For the QML static-tree flow, runtime JS is built from TypeScript as ESM with a **content hash**.
- Keep the manifest contract in `ts/public/js/static/qml-static-manifest.json`.
- Do not add legacy-browser bootstrap shims to this flow.
- The source of truth for UI structure is a typed C++ static tree, not JSON.

### 9. Exception-free error handling

- The entire repository is moving towards an **exception-free** architecture (compiling with `-fno-exceptions`).
- Do not use `throw`, `try`, or `catch` in new code.
- Use `std::expected` (C++23/26) for functions that can fail, returning errors as values.
- For Asio coroutines, prefer returning `asio::awaitable<std::expected<T, Error>>` or using `std::error_code` instead of throwing exceptions.

---

## Subsystem map

| Path | Purpose | Key constraint |
|------|---------|----------------|
| `src/Wt/cpp26/reflection.hpp` | Compile-time widget discovery, dispatch, auto-JSON | C++26 only, no fallbacks |
| `src/Wt/cpp26/WWidget-impl.hpp` | WWidget reflection integration | Keep in sync with reflection.hpp |
| `src/Wt/cuehttp/` | HTTP server (header-only 1.1 in libwt) | Zero nghttp2/lsquic includes |
| `src/Wt/cuehttp/detail/connection.hpp` | HTTP/1.1 base connection (CRTP) | Header-only, no nghttp2 |
| `src/Wt/cuehttp/detail/h2_mixin.hpp` | CRTP mixin with all nghttp2 callbacks | libwhttp only |
| `src/Wt/cuehttp/detail/h2_connection.hpp` | HTTP/2 connection (= base + h2_mixin) | libwhttp only |
| `src/Wt/cuehttp/detail/h2_flush_impl.C` | HTTP/2 async flush | co_await only, libwhttp |
| `src/Wt/cuehttp/server_h2.hpp` | Convenience alias for HTTP/2 server | libwhttp only |
| `src/Wt/cuehttp/detail/h3_stream_adapter.hpp` | Bridges nexus h3 stream → context | libwhttp only, header-only |
| `src/Wt/cuehttp/h3_server.hpp` | HTTP/3 server declaration (pimpl) | libwhttp only, no SSL includes |
| `src/Wt/cuehttp/h3_server.C` | HTTP/3 server impl (BoringSSL prefix) | Compiled with `BORINGSSL_PREFIX=BSSL` |
| `src/Wt/cuehttp/nexus/` | QUIC/HTTP3 transport (lsquic) | Compiled into libwhttp |
| `src/3rdparty/nghttp2-src/` | nghttp2 v1.64.0 (static PIC) | Built from source, embedded |
| `src/3rdparty/lsquic-src/` | lsquic + BoringSSL merged bundle | `liblsquic_bundle.a`, BSSL_ prefixed |
| `src/3rdparty/boringssl-src/` | BoringSSL (BSSL_ prefix) | Built with `BORINGSSL_PREFIX=BSSL` |
| `src/Wt/cuehttp/detail/MoveOnlyFunction.hpp` | Type-erased callable | Replaces std::function where needed |
| `src/Wt/Dbo/` | ORM | All operations async, no variant |
| `src/Wt/Dbo/SqlConnection.h` | Connection interface | Unified, non-variant |
| `src/Wt/Dbo/Session.C` | Session management | co_await throughout |
| `src/Wt/cpp26/qml_static_bootstrap_poc.hpp` | Static UI tree bootstrap PoC | Experimental contract, no legacy fallback |
| `src/Wt/cpp26/generated/` | Generated static UI trees from QML-like input | Generated files, do not hand-edit |
| `src/glaze/` | Glaze JSON library (bundled) | Do not modify; update version wholesale |
| `src/Wt/CLI11.hpp` | CLI argument parsing (bundled) | Do not modify |
| `Wasm/CMakeLists.txt` | Emscripten client build | Mirrors src/ structure |
| `ts/` | TypeScript widget JS bridges | Keep in sync with server-side widget API |
| `ts/src/qml-static/bootstrap.ts` | Static-tree action runtime | ESM-only, action bridge via `data-wt-action` |
| `ts/scripts/build-static-runtime.mjs` | Hashed runtime build + manifest | Output must stay deterministic by content |
| `tools/qml_static_codegen.py` | QML-like DSL to C++ static tree generator | Keep DSL simple and deterministic |
| `docs/adr/0001-stateless-session-static-ui-tree.md` | Session/tree boundary decision | Keep README in sync on user-facing changes |
| `cmake/WtFindBoost.cmake` | Boost discovery | Replaces old WtFindBoost.txt |

---

## Architecture decisions to know

### Widget serialization wire format

When a widget is serialized for transfer (server → WASM or for debugging):

```
WPushButton\n
{"text":"Click me","enabled":true,...}
```

Line 1: the **type tag** — `identifier_of` the concrete class.
Line 2+: Glaze JSON of the object.

`read_with_header_factory<Base>(blob)` reconstructs the concrete object. Any new widget class is automatically supported via reflection — no registration needed.

### Dispatch pattern

To call a function on a `WWidget*` with the actual concrete type:

```cpp
Wt::cpp26::dispatch(*widget, []<class T>(T& w) {
    // w is the concrete type here
    render(w);
});
```

This uses `template for` over all reflected `WWidget` subclasses. Order is most-derived first.

### DBO async pattern

```cpp
// Correct
auto result = co_await session.find<User>().where("name = ?").bind(name).resultList();

// Wrong — do not use sync wrappers
auto result = session.findSync<User>(...); // don't add this
```

### HTTP/1.1 header-only constraint

Any new HTTP/1.1 logic must live in a `.hpp` file inside `src/Wt/cuehttp/`. If it needs a TU (e.g. for HTTP/2), add a `.C` file and note it as HTTP/2+ only.

### Static-tree bootstrap contract (PoC)

`docs/adr/0001-stateless-session-static-ui-tree.md` defines the experimental contract:

- Session is execution context, not the owner of UI tree shape.
- Initial UI tree comes from typed C++ static nodes generated from QML.
- JSON snapshots are optional for debug/transport, never canonical.
- Runtime is shipped as a content-hashed ESM file with a manifest.

### Stateless Sessions & Distributed State

The traditional Wt `WApplication` stateful session is dead. In this fork:
- **Business data only:** Server-side sessions store only lightweight business context (e.g., `user_id`, `roles`, workflow state), never UI widgets.
- **Zero-boilerplate serialization:** Session structs are serialized to/from JSON using **Glaze** and C++26 reflection (`<meta>`).
- **Distributed storage ready:** Sessions are designed to be fetched and stored in external key-value stores (like Redis) via `co_await`. This enables horizontal scalability, load balancing without sticky sessions, and instant failover.
- **On-demand hydration:** When a WASM client sends a JSON diff for a backend action, the server asynchronously hydrates the session context, processes the action, and persists the updated session back to the store.

---

## What does NOT exist yet (do not assume)

- QML compiler / parser — not started
- HTTP/3 end-to-end test — server listens on UDP, needs curl built with HTTP/3 (lsquic + BoringSSL) to verify
- Reflection-driven DBO schema generation — in design
- Stateless `WApplication` redesign — in design (bootstrap/tree PoC exists, full migration not done)
- WASM ↔ server state sync protocol — in design

Do not invent APIs for these. If asked to work on one, ask for a design spec first.

---

## Build

```bash
# Server target with HTTP/2+3 (requires Clang P2996 branch)
cmake -B build -DBUILD_WASM=OFF -DENABLE_HTTP2=ON -DENABLE_HTTP3=ON \
  -DBoost_DIR=/usr/local/lib/cmake/Boost-1.88.0 \
  -DCMAKE_CXX_COMPILER=/opt/clang-p2996/bin/clang++ \
  -DCMAKE_CXX_STANDARD=26
cmake --build build

# With WASM client (requires Emscripten)
cmake -B build -DCMAKE_CXX_STANDARD=26 -DBUILD_WASM=ON
cmake --build build
```

### 3rdparty dependencies (HTTP/2+3)

All protocol dependencies are **statically embedded** in `libwhttp.so`. Users link `libwhttp` and get HTTP/1.1+2+3 without installing nghttp2, lsquic, or BoringSSL.

| Dependency | Location | Build notes |
|------------|----------|-------------|
| **nghttp2** v1.64.0 | `src/3rdparty/nghttp2-src/` | Built with `-DCMAKE_POSITION_INDEPENDENT_CODE=ON -DENABLE_LIB_ONLY=ON` |
| **lsquic** | `src/3rdparty/lsquic-src/` | Built against prefixed BoringSSL via `EXTRA_CFLAGS` |
| **BoringSSL** | `src/3rdparty/boringssl-src/` | Built with `-DBORINGSSL_PREFIX=BSSL` — all 3500+ symbols prefixed (`SSL_new` → `BSSL_SSL_new`) |

**BoringSSL symbol prefix**: lsquic requires BoringSSL, but `libwhttp.so` also links system OpenSSL (for HTTP/1.1+2 TLS). Both export identical symbols (`SSL_new`, `SSL_CTX_new`, etc.). The `BORINGSSL_PREFIX` mechanism renames all BoringSSL symbols with a `BSSL_` prefix, completely eliminating conflicts. lsquic + BoringSSL `.o` files are merged into `liblsquic_bundle.a`.

**Pimpl for h3_server**: `h3_server.C` is compiled with `BORINGSSL_PREFIX=BSSL` (same as nexus files). All `SSL_CTX` creation happens inside the `.C` file, never in headers. This prevents callers from accidentally creating an OpenSSL `SSL_CTX` where BoringSSL is expected.

**Dynamic deps** of `libwhttp.so` at runtime: system OpenSSL (libssl.so.3, libcrypto.so.3), Boost 1.88, zlib, libstdc++, libc.

---

## When to update this file

Update `agents.md` when:
- A new subsystem is started or a major decision is made (e.g. simdjson adopted, HTTP/3 started).
- A new bundled library is added.
- A core rule changes (e.g. a new C++26 feature becomes mandatory).
- The wire format or dispatch pattern changes.

Update `README.md` at the same time if the change is user-facing.
