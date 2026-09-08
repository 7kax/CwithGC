# TODO

This file contains unfinished, actionable work only. Architectural decisions and C feature-support
policy are documented in `docs/architecture.md`.

## Priority Definitions

- **P0**: Undefined behavior, data corruption, or errors in core GC logic
- **P1**: Work required for a reliable compiler/runtime pipeline, ABI safety, or portability
- **P2**: Testing depth, optional feature support, build improvements, or maintainability

## Roadmap

| Status | Phase | Priority | Module | Work | Completion Criteria |
| --- | --- | --- | --- | --- | --- |
| [ ] | 3 | P1 | compiler / integration | Implement a minimal automatic instrumentation pipeline | C inputs containing no hand-written GC calls are lowered, compiled, linked, and executed against all three collectors; lowering emits static v2 type descriptors and typed object/array allocations, enforces the alignment, pointer-representation, nonzero-array, and allocation-base limits, sequences field-slot formation after safe points, and fixtures cover scalars, structs, fixed and dynamic arrays, calls, recursion, and multiple returns |
| [ ] | 4 | P2 | compiler | Add compatible C features incrementally | Each feature listed as planned in `docs/architecture.md` is enabled separately with positive lowering tests and diagnostics for forms that remain unsupported |
| [ ] | 5 | P2 | tests | Add model-based cross-collector conformance testing | Generated valid object-graph operations preserve values and reachability across collectors; address movement and reference-cycle retention are treated as documented collector-specific behavior |
