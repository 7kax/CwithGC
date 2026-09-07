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
| [ ] | 2 | P1 | compiler / ABI | Specify the supported C subset and freeze the v1 lowering contract | Documentation defines allocation typing, safe points, root lifetimes, object-layout generation, control-flow cleanup, diagnostics, and the rule that exactly one collector implementation is linked |
| [ ] | 2 | P1 | compiler / metadata | Define canonical compiler-generated pointer tables | Each supported object type produces unique aligned offsets and the correct element count; compiler tests compare emitted layouts with C `offsetof` and `sizeof` results; duplicate offsets cannot make reference counting visit one field twice |
| [ ] | 3 | P1 | compiler / integration | Implement a minimal automatic instrumentation pipeline | C inputs containing no hand-written GC calls are lowered, compiled, linked, and executed against all three collectors; fixtures cover scalars, structs, fixed arrays, calls, recursion, and multiple returns |
| [ ] | 4 | P2 | compiler | Add compatible C features incrementally | Each feature listed as planned in `docs/architecture.md` is enabled separately with positive lowering tests and diagnostics for forms that remain unsupported |
| [ ] | 5 | P2 | tests | Add model-based cross-collector conformance testing | Generated valid object-graph operations preserve values and reachability across collectors; address movement and reference-cycle retention are treated as documented collector-specific behavior |
