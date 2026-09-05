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
| [ ] | 4 | P2 | collectors | Refactor collectors one at a time in the order `ref_count` -> `copying` -> `mark_sweep` | Each step is an independently reviewable commit; focused tests, the full suite, and sanitizers pass |
| [ ] | 4 | P2 | CMake / CI | Add C ABI smoke tests, strict warnings, clang-format checks, and sanitizer checks | C and C++ callers are covered by automated tests; new code passes formatting and the agreed warning/sanitizer configurations |

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
| [x] | P1 | all | Make `gc_pop()` return safely when the root set is empty | The implementation no longer calls `back()` on an empty vector |
| [ ] | P1 | all | Redesign root lifetime management to reduce reliance on `__builtin_frame_address(1)` | Behavior is stable under optimized builds, different compilers, and recursive calls; preferably use an explicit scope/token API |
| [ ] | P1 | all | Define ownership and call requirements for `gc_local_var()` and `gc_ptr_copy()` | Documentation states which pointers must be registered and which fields must be updated through `gc_ptr_copy()` |
| [x] | P1 | ref_count | Make `gc_cleanup()` release live objects, or explicitly require callers to release every reference first | LeakSanitizer reports no remaining GC objects; state is consistent after cleanup |
| [ ] | P1 | ref_count | Define or implement collection of reference cycles | Documentation explicitly states that cycles are not collected, or cycle-collector tests and implementation are added |
| [x] | P1 | all | Validate pointer-table ranges, offsets, array lengths, and object payload sizes in `gc_register()` | Invalid pointer tables are rejected and cannot cause out-of-bounds access during mark, copy, or decrement operations |
| [x] | P1 | all | Define pointer-table ownership and lifetime so metadata cannot retain dangling table pointers | A pointer table remains valid for the object's lifetime and does not leak auxiliary memory |

## Memory Layout and Portability

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P1 | gc.h / all | Replace `u_int8_t` and `u_int64_t` with standard types and avoid representing pointer arithmetic directly as integers | Use `uint8_t`, `uintptr_t`, or standard byte-pointer arithmetic |
| [x] | P1 | gc.h | Redesign `positions[0]` to avoid relying on the nonstandard zero-length array extension in C and C++ | Target C11 and C++20 compilers do not depend on nonstandard extensions |
| [x] | P1 | all | Avoid integer comparisons and arithmetic on object pointers; use safe byte pointers and bounds checks consistently | Behavior is defined under UBSan, strict compilers, and 32-bit and 64-bit environments |
| [ ] | P1 | debug API | Standardize allocation and deallocation for `gc_mem_layout()`; the implementation currently uses `new[]` while tests use `free()` | ASan no longer reports an allocation/deallocation mismatch; preferably provide `gc_mem_layout_free()` |
| [ ] | P1 | ref_count | Decide whether `gc_mem_layout()` supports the reference-counting implementation instead of exposing an API that always returns `nullptr` | Documentation and implementation agree, and generic debugging code cannot misuse the interface |

## Testing and Validation

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [ ] | P1 | tests | Add regression tests for nested objects, cycles, repeated collections, full-heap allocation, and self-assignment | Every fixed core bug has a minimal reproducing test |
| [ ] | P1 | tests | Add ASan, UBSan, and LeakSanitizer build/test configurations | CI or a local command can run sanitizer tests in one step |
| [ ] | P2 | verify | Replace dangerous examples hidden in unused `_main()` functions with tests that actually detect dangling pointers, double frees, and use-after-free | Verification cases detect the expected signal or sanitizer report instead of checking only for normal exit |
| [ ] | P2 | tests | Avoid relying only on `assert` so Release builds still check results | Tests fail correctly when `NDEBUG` is defined |
| [x] | P2 | tests | Free dynamically allocated pointer tables in tests or replace them with static constant tables | LeakSanitizer reports no leaks from test helper memory |

## Build and Code Quality

| Status | Priority | Module | TODO | Completion Criteria |
| --- | --- | --- | --- | --- |
| [x] | P2 | CMake | Replace global `include_directories()` with `target_include_directories()` and `target_link_libraries(... PRIVATE ...)` | Target dependency boundaries are explicit and directories do not pollute one another |
| [ ] | P2 | CMake | Add a `BUILD_TESTING` option and separate ordinary unit tests from verify cases that intentionally trigger errors | Default builds are controllable and negative tests do not masquerade as ordinary passing tests |
| [ ] | P2 | gc.h / CMake | Stop defining `GC_DEBUG` unconditionally in the public header | Build configuration determines whether the debug API is enabled |
| [ ] | P2 | gc.h | Document failure behavior, thread safety, and lifecycle requirements | C compilers can check calls strictly and the API contract is complete |
| [ ] | P2 | all | Enforce the clang-format style and the English-only documentation/comment policy | Formatting and language checks can run automatically |

## Current Validation Baseline

- [x] Standard Clang build passes
- [x] Current CTest result: 84/84 passing
- [ ] Full ASan/UBSan test suite passes
- [x] Repeated-GC nested-object tests pass
- [ ] Release (`NDEBUG`) tests pass

## Confirmed Issues Discovered During Iteration

- [ ] **Root-frame lookup triggers strict warnings**: Clang rejects `__builtin_frame_address(1)` with `-Wframe-address` under `-Wall -Wextra -Wpedantic -Werror`. The strict build currently requires the temporary `-Wno-error=frame-address` workaround; resolve this as part of the root-lifetime redesign.
- [ ] **Full sanitizer validation remains blocked by the debug API**: Phase 3 validated 34 safety tests with ASan, UBSan, and LeakSanitizer. Full CTest coverage still requires fixing the `new[]`/`free()` allocation mismatch in `gc_mem_layout()`.
- [ ] **Post-cleanup pointer invalidation must be documented**: `gc_cleanup()` releases all GC memory, including live objects. Every GC pointer held by a caller becomes invalid afterward; document this lifecycle boundary in the C API.
