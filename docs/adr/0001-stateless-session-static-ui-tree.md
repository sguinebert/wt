# ADR-0001: Stateless Session Boundary And Static UI Tree Bootstrap

- Date: 2026-02-20
- Status: Proposed (PoC implemented)

## Context

The current Wt boot flow is historically tied to server-managed session state and dynamic bootstrap scripts.  
For the QML-to-C++ direction, we need a deterministic startup path where the initial UI tree is generated statically from compiled input and hydrated with a modern JavaScript runtime.

Project constraints already push in this direction:

- C++26-first architecture with reflection-driven metadata.
- Breaking changes are expected.
- TypeScript runtime is maintained in-repo and should be delivered as modern ESM.

## Decision

We standardize an experimental bootstrap contract for QML static trees:

1. Session boundary
- Session is execution context (identity, auth, subscriptions, action handling).
- Session is not the owner of the UI tree shape.

2. Initial tree
- QML is compiled to a typed **C++ static widget tree** (the source of truth).
- The initial widget graph is instantiated directly in C++ (no JSON required for tree construction).
- Optional JSON snapshots are allowed for debug/transport, but must not become the canonical model.

3. Runtime distribution
- Runtime JS is built from TypeScript into a content-hashed ESM file.
- A manifest maps logical runtime id to the hashed file.
- Assets are designed for immutable caching.

4. Interaction contract
- Client interaction emits actions (`action id + payload`) to one endpoint.
- Server can later answer with state/diff messages (not fully implemented in this ADR).

## PoC Scope In This Repository

- C++ static tree model and widget instantiation helper:
  - `src/Wt/cpp26/qml_static_bootstrap_poc.hpp`
- QML-like input to C++ static tree code generator:
  - `tools/qml_static_codegen.py`
- TypeScript action-bridge runtime:
  - `ts/src/qml-static/bootstrap.ts`
- Static runtime build and manifest generator:
  - `ts/scripts/build-static-runtime.mjs`

This ADR documents the shape and delivery model, not a full replacement of the legacy session stack.

## Consequences

Positive:

- Deterministic and testable startup path.
- No legacy browser bootstrap assumptions required for this flow.
- Clean seam between compile-time UI description (C++) and runtime state transitions.

Tradeoffs:

- Two boot paths coexist during migration (legacy Wt bootstrap and static-tree bootstrap).
- Manifest + hashed asset management becomes mandatory for this flow.
- Server/session APIs must be progressively refactored to consume action/state protocols.

## Follow-Up

1. Define canonical action and patch schema (`actions -> state -> patch`).
2. Add one real endpoint that consumes PoC actions asynchronously.
3. Bind a first generated QML artifact to this static C++ tree builder.
