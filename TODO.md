# TODO

## Priority Definitions

- **P0**: Undefined behavior, data corruption, or errors in core GC logic
- **P1**: API safety, portability, or resource-management issues
- **P2**: Testing, build, and maintainability improvements

## Architecture Direction: C API with a C++20 Implementation

- Keep `include/gc.h` compatible with C11 and use it as the sole public interface. Keep tests in C to continuously verify the real C ABI.
- Implement internals under `src/` in C++20, using RAII, standard containers, templates, and type-safe helper abstractions where they improve readability.
- Do not expose STL types, templates, references, exceptions, or other C++ types through the public ABI. Exceptions must never cross an `extern "C"` boundary.

### Optimization Plan

| Status | Phase | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- | --- |
| [x] | 1 | P1 | gc.h / ABI | Establish a pure C interface boundary and add minimal C11 and C++20 caller compilation tests | The same `gc.h` compiles under strict C11 and C++20; exported symbols retain C linkage; public declarations do not depend on C++ types |
| [x] | 1 | P1 | all | Standardize exception boundaries for exported functions and convert C++ exceptions into explicit failure behavior | Exceptions such as `std::bad_alloc` never cross `extern "C"`; C callers observe only documented return values or termination behavior |
| [x] | 2 | P1 | all | Encapsulate each collector's heap, roots, free list, and statistics in one internal state object | Scattered mutable globals are removed; initialization, collection, and cleanup maintain invariants through the state object |
| [x] | 2 | P1 | all | Use RAII for heap buffers and internal helper resources while retaining the explicit `gc_init()` / `gc_cleanup()` C API | Partial initialization failures and repeated cleanup do not leak; resource release does not depend on manually maintained branches |
| [x] | 3 | P2 | common | Extract shared `RootSet`, memory-layout, pointer-table validation, and fatal-error components | All three implementations reuse the same foundation; duplicate logic is removed without changing collector semantics |
| [x] | 3 | P2 | all | Replace raw integer address arithmetic and repeated casts with `std::byte`, checked ranges, and small helper types | Core scanning code directly expresses blocks, payloads, and field slots; bounds checks are centralized and pointer arithmetic has no undefined behavior |
| [x] | 4 | P2 | collectors | Refactor collectors one at a time in the order `ref_count` -> `copying` -> `mark_sweep` | Each step is an independently reviewable commit; focused tests, the full suite, and sanitizers pass |
| [x] | 4 | P2 | CMake / CI | Add C ABI smoke tests, strict warnings, clang-format checks, and sanitizer checks | C and C++ callers are covered by automated tests; new code passes formatting and the agreed warning/sanitizer configurations |

## Core Correctness

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P0 | copying | After copying an object, update every pointer inside the to-space object instead of modifying the from-space source | Nested objects, linked lists, and trees contain only pointers into the new space after GC; repeated collections continue to pass |
| [x] | P0 | ref_count | Fix the use-after-free caused by reading `meta_ptr->size` after freeing metadata | `ref_count_basic` and `ref_count_recursion` report no UAF under ASan |
| [x] | P0 | all | Apply maximum alignment to every allocation block and standardize metadata, payload, and free-block address calculations | UBSan alignment checks report no errors; objects of varying sizes can be allocated safely |
| [x] | P0 | mark_sweep | Handle `free_list == nullptr` correctly so sweep does not assert when the heap is full or has no free blocks | Calling `gc_collect()` after filling the heap does not crash; allocation failure follows the common failure path |
| [x] | P0 | ref_count | Handle self-assignment and release ordering in `gc_ptr_copy(dst, src)` so the old reference is not released before the new one is retained | `gc_ptr_copy(&p, p)`, aliased assignments, and field replacements are safe under ASan |
| [x] | P0 | all | Check integer overflow in `size + sizeof(meta_data)` and pointer-table size calculations | Oversized requests are rejected instead of wrapping to a small allocation and writing out of bounds |

## Lifecycle and API

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P1 | all | Add initialization-state checks to `gc_init()`, `gc_collect()`, `gc_malloc()`, and `gc_cleanup()`, including repeated initialization | Calls before initialization, repeated initialization, and calls after cleanup all have defined behavior |
| [x] | P1 | all | Use explicit scope tokens for root lifetime management | All roots are attached to explicit nested scopes, and behavior is stable under optimized builds, different compilers, and recursive calls |
| [x] | P1 | all | Define ownership and call requirements for `gc_local_var()` and `gc_ptr_copy()` | Documentation states which pointers must be registered and which fields must be updated through `gc_ptr_copy()` |
| [x] | P1 | ref_count | Make `gc_cleanup()` release live objects, or explicitly require callers to release every reference first | LeakSanitizer reports no remaining GC objects; state is consistent after cleanup |
| [x] | P1 | ref_count | Define or implement collection of reference cycles | Documentation explicitly states that cycles are not collected, or cycle-collector tests and implementation are added |
| [x] | P1 | all | Validate pointer-table ranges, offsets, array lengths, and object payload sizes in `gc_register()` | Invalid pointer tables are rejected and cannot cause out-of-bounds access during mark, copy, or decrement operations |
| [x] | P1 | all | Define pointer-table ownership and lifetime so metadata cannot retain dangling table pointers | A pointer table remains valid for the object's lifetime and does not leak auxiliary memory |

## Memory Layout and Portability

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P1 | gc.h / all | Replace `u_int8_t` and `u_int64_t` with standard types and avoid representing pointer arithmetic directly as integers | Use `uint8_t`, `uintptr_t`, or standard byte-pointer arithmetic |
| [x] | P1 | gc.h | Redesign `positions[0]` to avoid relying on the nonstandard zero-length array extension in C and C++ | Target C11 and C++20 compilers do not depend on nonstandard extensions |
| [x] | P1 | all | Avoid integer comparisons and arithmetic on object pointers; use safe byte pointers and bounds checks consistently | Behavior is defined under UBSan, strict compilers, and 32-bit and 64-bit environments |
| [x] | P1 | debug API | Standardize allocation and deallocation for `gc_mem_layout()` | Callers release layouts through `gc_mem_layout_free()`; ASan reports no allocation/deallocation mismatch |
| [ ] | P1 | ref_count | Decide whether `gc_mem_layout()` supports the reference-counting implementation instead of exposing an API that always returns `nullptr` | Documentation and implementation agree, and generic debugging code cannot misuse the interface |

## Testing and Validation

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [ ] | P1 | tests | Add regression tests for nested objects, cycles, repeated collections, full-heap allocation, and self-assignment | Every fixed core bug has a minimal reproducing test |
| [x] | P1 | tests | Add ASan, UBSan, and LeakSanitizer build/test configurations | CI or a local command can run sanitizer tests in one step |
| [ ] | P2 | verify | Add negative tests that actually detect dangling pointers, double frees, and use-after-free; the unused `_main()` examples have been removed | Verification cases detect the expected signal or sanitizer report instead of checking only for normal exit |
| [ ] | P2 | tests | Avoid relying only on `assert` so Release builds still check results | Tests fail correctly when `NDEBUG` is defined |
| [x] | P2 | tests | Free dynamically allocated pointer tables in tests or replace them with static constant tables | LeakSanitizer reports no leaks from test helper memory |

## Build and Code Quality

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P2 | CMake | Replace global `include_directories()` with `target_include_directories()` and `target_link_libraries(... PRIVATE ...)` | Target dependency boundaries are explicit and directories do not pollute one another |
| [x] | P2 | CMake | Add a `BUILD_TESTING` option and keep ordinary unit tests separate from verification cases | Default builds are controllable and library-only builds exclude all test targets |
| [x] | P2 | gc.h / CMake | Stop defining `GC_DEBUG` unconditionally in the public header | `GC_ENABLE_DEBUG_API` determines whether the debug API is declared and built |
| [x] | P2 | gc.h | Document failure behavior, thread safety, and lifecycle requirements | C compilers can check calls strictly and the API contract is complete |
| [x] | P2 | all | Enforce the clang-format style and the English-only documentation/comment policy | The `quality-check` target runs formatting and language checks automatically |

## Current Validation Baseline

- [x] Standard Clang build passes
- [x] Current CTest result: 88/88 passing
- [x] Full ASan/UBSan test suite passes
- [x] Repeated-GC nested-object tests pass
- [ ] Release (`NDEBUG`) tests pass

## Confirmed Issues Discovered During Iteration

- [x] **Root-frame lookup triggers strict warnings**: The explicit scope/token API removes `__builtin_frame_address(1)`, so strict builds no longer need the temporary `-Wno-error=frame-address` workaround.
- [x] **Full sanitizer validation is enabled**: The `sanitizers` preset runs all 88 tests with ASan, UBSan, and LeakSanitizer after fixing the `gc_mem_layout()` allocation/deallocation contract.
- [x] **Post-cleanup pointer invalidation must be documented**: `gc_cleanup()` releases all GC memory, including live objects. Every GC pointer held by a caller becomes invalid afterward; the C API documents this lifecycle boundary.
