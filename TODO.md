# TODO

## Priority Definitions

- **P0**: Undefined behavior, data corruption, or errors in core GC logic
- **P1**: API safety, portability, or resource-management issues
- **P2**: Testing, build, and maintainability improvements

## Architecture Direction: Compiler/Instrumentation C ABI with a C++20 Implementation

- Keep `include/gc.h` as the C11 ABI boundary consumed by compiler-generated or compiler-inserted instrumentation, not as a manually called application API. Keep `include/gc_debug.h` as the stable optional inspection interface, and keep C/C++ runtime-ABI smoke tests to verify the real boundary.
- Implement internals under `src/` in C++20, using RAII, standard containers, templates, and type-safe helper abstractions where they improve readability.
- Do not expose STL types, templates, references, exceptions, or other C++ types through the public ABI. Exceptions must never cross an `extern "C"` boundary.

### Optimization Plan

| Status | Phase | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- | --- |
| [x] | 1 | P1 | gc.h / ABI | Establish a pure C runtime-ABI boundary and add minimal C11 and C++20 compilation tests | The same `gc.h` compiles under strict C11 and C++20; exported symbols retain C linkage; ABI declarations do not depend on C++ types |
| [x] | 1 | P1 | all | Standardize exception boundaries for exported functions and convert C++ exceptions into explicit failure behavior | Exceptions such as `std::bad_alloc` never cross `extern "C"`; generated code observes only documented return values or termination behavior |
| [x] | 2 | P1 | all | Encapsulate each collector's heap, roots, free list, and statistics in one internal state object | Scattered mutable globals are removed; initialization, collection, and cleanup maintain invariants through the state object |
| [x] | 2 | P1 | all | Use RAII for heap buffers and internal helper resources while retaining the explicit `gc_init()` / `gc_cleanup()` runtime ABI | Partial initialization failures and repeated cleanup do not leak; resource release does not depend on manually maintained branches |
| [x] | 3 | P2 | common | Extract shared `RootSet`, memory-layout, pointer-table validation, and fatal-error components | All three implementations reuse the same foundation; duplicate logic is removed without changing collector semantics |
| [x] | 3 | P2 | all | Replace raw integer address arithmetic and repeated casts with `std::byte`, checked ranges, and small helper types | Core scanning code directly expresses blocks, payloads, and field slots; bounds checks are centralized and pointer arithmetic has no undefined behavior |
| [x] | 4 | P2 | collectors | Refactor collectors one at a time in the order `ref_count` -> `copying` -> `mark_sweep` | Each step is an independently reviewable commit; focused tests, the full suite, and sanitizers pass |
| [x] | 4 | P2 | CMake / CI | Add C ABI smoke tests, strict warnings, clang-format checks, and sanitizer checks | C and C++ runtime-ABI smoke inputs are covered by automated tests; new code passes formatting and the agreed warning/sanitizer configurations |

## Future Work

- [ ] **P2 | compiler/instrumentation**: Add an in-repo automatic instrumentation pass that lowers program startup and teardown, managed allocations, pointer assignments, object-layout registration, and local-root scopes to the `gc.h` runtime ABI. A checked-in tool should transform a supported input program and produce generated code that exercises the runtime ABI without hand-written GC calls.

## Core Correctness

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P0 | copying | After copying an object, update every pointer inside the to-space object instead of modifying the from-space source | Nested objects, linked lists, and trees contain only pointers into the new space after GC; repeated collections continue to pass |
| [x] | P0 | ref_count | Fix the use-after-free caused by reading `header->size` after freeing metadata | `ref_count_basic` and `ref_count_recursion` report no UAF under ASan |
| [x] | P0 | all | Apply maximum alignment to every allocation block and standardize metadata, payload, and free-block address calculations | UBSan alignment checks report no errors; objects of varying sizes can be allocated safely |
| [x] | P0 | mark_sweep | Handle `free_list == nullptr` correctly so sweep does not assert when the heap is full or has no free blocks | Calling `gc_collect()` after filling the heap does not crash; allocation failure follows the common failure path |
| [x] | P0 | ref_count | Handle self-assignment and release ordering in `gc_pointer_assign(destination_slot, source)` so the old reference is not released before the new one is retained | `gc_pointer_assign(&p, p)`, aliased assignments, and field replacements are safe under ASan |
| [x] | P0 | all | Check integer overflow in `size + sizeof(ObjectHeader)` and pointer-table size calculations | Oversized requests are rejected instead of wrapping to a small allocation and writing out of bounds |

## Lifecycle and API

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P1 | all | Add initialization-state checks to `gc_init()`, `gc_collect()`, `gc_malloc()`, and `gc_cleanup()`, including repeated initialization | Calls before initialization, repeated initialization, and calls after cleanup all have defined behavior |
| [x] | P1 | all | Use explicit scope tokens for root lifetime management | All roots are attached to explicit nested scopes, and behavior is stable under optimized builds, different compilers, and recursive calls |
| [x] | P1 | all | Define ownership and call requirements for `gc_scope_add_root()` and `gc_pointer_assign()` | Documentation states which pointers must be registered and which fields must be updated through `gc_pointer_assign()` |
| [x] | P1 | ref_count | Make `gc_cleanup()` release live objects without requiring generated code to clear every reference first | LeakSanitizer reports no remaining GC objects; state is consistent after cleanup |
| [x] | P1 | ref_count | Define or implement collection of reference cycles | Documentation explicitly states that cycles are not collected, or cycle-collector tests and implementation are added |
| [x] | P1 | all | Validate pointer-table ranges, offsets, array lengths, and object payload sizes in `gc_register_object()` | Invalid pointer tables are rejected and cannot cause out-of-bounds access during mark, copy, or decrement operations |
| [x] | P1 | all | Define pointer-table ownership and lifetime so metadata cannot retain dangling table pointers | A pointer table remains valid for the object's lifetime and does not leak auxiliary memory |

## Memory Layout and Portability

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P1 | gc.h / all | Replace `u_int8_t` and `u_int64_t` with standard types and avoid representing pointer arithmetic directly as integers | Use `uint8_t`, `uintptr_t`, or standard byte-pointer arithmetic |
| [x] | P1 | gc.h | Redesign the first pointer-field offset to avoid relying on the nonstandard zero-length array extension in C and C++ | Target C11 and C++20 compilers do not depend on nonstandard extensions |
| [x] | P1 | all | Avoid integer comparisons and arithmetic on object pointers; use safe byte pointers and bounds checks consistently | Behavior is defined under UBSan, strict compilers, and 32-bit and 64-bit environments |
| [x] | P1 | debug API | Standardize allocation and deallocation for memory-layout snapshots | Snapshots carry an explicit block count and are cleared by `gc_debug_memory_layout_dispose()`; ASan reports no allocation/deallocation mismatch |
| [x] | P1 | ref_count | Implement memory inspection as a live-allocation snapshot | The API documents noncontiguous allocation and free-capacity semantics; generic inspection code can consume and release the snapshot safely |

## Testing and Validation

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P1 | tests | Add regression tests for nested objects, cycles, repeated collections, full-heap allocation, and self-assignment | Every fixed core bug has a minimal reproducing test |
| [x] | P1 | tests | Add ASan, UBSan, and LeakSanitizer build/test configurations | CI or a local command can run sanitizer tests in one step |
| [x] | P2 | tests | Replace the ambiguous verification tree with ABI, conformance, failure, and inspection test labels | Tests model compiler-generated calls; impossible malformed-instrumentation scenarios are removed; reachable failures validate both termination and diagnostics |
| [ ] | P2 | tests | Avoid relying only on `assert` so Release builds still check results | Tests fail correctly when `NDEBUG` is defined |
| [x] | P2 | tests | Free dynamically allocated pointer tables in tests or replace them with static constant tables | LeakSanitizer reports no leaks from test helper memory |

## Build and Code Quality

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P2 | CMake | Replace global `include_directories()` with `target_include_directories()` and `target_link_libraries(... PRIVATE ...)` | Target dependency boundaries are explicit and directories do not pollute one another |
| [x] | P2 | CMake | Add a `BUILD_TESTING` option and keep all runtime tests in one labeled suite | Default builds are controllable, library-only builds exclude tests, and no separate verification tree is required |
| [x] | P2 | debug API / CMake | Replace the public `GC_DEBUG` macro contract with a stable optional inspection API | `gc_debug.h` is always usable; `GC_ENABLE_INSPECTION` is private to the library build, and disabled builds return explicit status codes |
| [x] | P2 | gc.h | Document failure behavior, thread safety, and lifecycle requirements | C compilers can check generated calls strictly and the runtime-ABI contract is complete |
| [x] | P2 | all | Enforce the clang-format style and the English-only documentation/comment policy | The `quality-check` target runs formatting and language checks automatically |
| [x] | P2 | all | Align the runtime C ABI, inspection statistics, and private C++ type names with their semantics | ABI operations use precise names, collector statistics distinguish reclaimed and relocated blocks, and internal types follow one naming convention |

## Current Validation Baseline

- [x] Standard Clang build passes
- [x] Current inspection-enabled CTest result: 58/58 passing
- [x] Inspection-disabled CTest result: 24/24 passing
- [x] Full ASan/UBSan test suite passes
- [x] Repeated-GC nested-object tests pass
- [ ] Release (`NDEBUG`) tests pass

## Confirmed Issues Discovered During Iteration

- [x] **Root-frame lookup triggers strict warnings**: The explicit scope/token API removes `__builtin_frame_address(1)`, so strict builds no longer need the temporary `-Wno-error=frame-address` workaround.
- [x] **Full sanitizer validation is enabled**: The `sanitizers` preset runs all 58 inspection-enabled tests with ASan, UBSan, and LeakSanitizer.
- [x] **Post-cleanup pointer invalidation must be documented**: `gc_cleanup()` releases all GC memory, including live objects. Every generated GC pointer becomes invalid afterward; the runtime ABI documents this lifecycle boundary.
