# TODO

## Priority Definitions

- **P0**: Undefined behavior, data corruption, or errors in core GC logic
- **P1**: API safety, portability, or resource-management issues
- **P2**: Testing, build, and maintainability improvements

## Architecture Direction: Compiler/Instrumentation C ABI with a C++20 Implementation

- Keep `include/gc.h` as the C11 ABI boundary consumed by compiler-generated or compiler-inserted instrumentation, not as a manually called application API. Keep `include/gc_debug.h` as the stable optional inspection interface, and keep C/C++ runtime-ABI smoke tests to verify the real boundary.
- Implement internals under `src/` in C++20, using RAII, standard containers, templates, and type-safe helper abstractions where they improve readability.
- Do not expose STL types, templates, references, exceptions, or other C++ types through the public ABI. Exceptions must never cross an `extern "C"` boundary.

## Future Work

- [ ] **P2 | compiler/instrumentation**: Add an in-repo automatic instrumentation pass that lowers program startup and teardown, managed allocations, pointer assignments, object-layout registration, and local-root scopes to the `gc.h` runtime ABI. A checked-in tool should transform a supported input program and produce generated code that exercises the runtime ABI without hand-written GC calls.

## Testing and Validation

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P1 | tests | Add regression tests for nested objects, cycles, repeated collections, full-heap allocation, and self-assignment | Every fixed core bug has a minimal reproducing test |
| [x] | P1 | tests | Add ASan, UBSan, and LeakSanitizer build/test configurations | CI or a local command can run sanitizer tests in one step |
| [x] | P2 | tests | Replace the ambiguous verification tree with ABI, conformance, failure, and inspection test labels | Tests model compiler-generated calls; impossible malformed-instrumentation scenarios are removed; reachable failures validate both termination and diagnostics |
| [ ] | P2 | tests | Avoid relying only on `assert` so Release builds still check results | Tests fail correctly when `NDEBUG` is defined |
| [x] | P2 | tests | Free dynamically allocated pointer tables in tests or replace them with static constant tables | LeakSanitizer reports no leaks from test helper memory |

## Current Validation Baseline

- [x] Standard Clang build passes
- [x] Current inspection-enabled CTest result: 58/58 passing
- [x] Inspection-disabled CTest result: 24/24 passing
- [x] Full ASan/UBSan test suite passes
- [x] Repeated-GC nested-object tests pass
- [ ] Release (`NDEBUG`) tests pass

## Confirmed Issues Discovered During Iteration
