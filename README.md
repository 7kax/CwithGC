# CwithGC

CwithGC is a small garbage-collection runtime. Its C11 compiler/instrumentation ABI is implemented
in C++20 with reference-counting, copying, and mark-and-sweep collectors.

See [the architecture document](docs/architecture.md) for the managed-pointer model, compiler
responsibilities, and the distinction between planned and fundamentally unsupported C features.
The precise generated-call contract is specified in [Compiler-Runtime ABI v2](docs/abi.md).

## Build and Test

Building the project requires CMake 3.20 or newer and Ninja. The checked-in CMake presets provide
the supported local validation configurations:

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Use the `strict` preset to treat compiler warnings as errors:

```sh
cmake --preset strict
cmake --build --preset strict
ctest --preset strict
```

The `release-tests` preset runs the complete test suite as a strict Release build, keeping test
checks active even when the compiler defines `NDEBUG`:

```sh
cmake --preset release-tests
cmake --build --preset release-tests
ctest --preset release-tests
```

The `sanitizers` preset additionally enables AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
```

Collector inspection is enabled by default. The `no-inspection` preset verifies that the stable
inspection API remains linkable and reports that the optional implementation is unavailable:

```sh
cmake --preset no-inspection
cmake --build --preset no-inspection
ctest --preset no-inspection
```

The library-only preset produces a strict Release build without inspection or tests:

```sh
cmake --preset library
cmake --build --preset library
```

The suite is organized by behavior rather than by a separate verification tree. Runtime-ABI smoke
tests use the `abi` label, successful lowered-program behavior uses `conformance`, reachable fatal
paths use `failure`, and tests that exercise optional collector inspection also use `inspection`.
For example:

```sh
ctest --test-dir build/default -L failure
```

The C sources under `test/` invoke runtime hooks explicitly only to model code that an automatic
instrumentation pass would emit.

## Compiler/Instrumentation ABI

The core runtime interface is a low-level C ABI (`include/gc.h`) between compiler-generated code (or
compiler-inserted instrumentation) and the runtime. It is not a general-purpose application
allocation API. A language implementation or instrumentation pass lowers managed allocation,
pointer-field updates, and local-root lifetimes to these ABI operations; application source should
not call them directly as a memory-management layer.

The runtime provides one process-global collector instance. It is single-threaded and not
thread-safe, so generated code and runtime integration must serialize initialization, allocation,
root management, pointer assignment, collection, and teardown. The generated program-start sequence
must invoke `gc_init()` before lifecycle-dependent operations. A repeated `gc_init()` restarts the
runtime, releasing the old state and invalidating its managed pointers and scope tokens.
`gc_cleanup()` is idempotent, releases all managed memory and active roots, and invalidates every
managed pointer. A subsequent generated execution must initialize the runtime again before using it.
Compiler-generated type descriptors use static storage and remain alive while the runtime can visit
allocations that reference them.

The following shows the shape of calls emitted around one instrumented local; it is an ABI smoke
sequence, not application code. Generated code opens and closes scopes in LIFO order, registers
each pointer slot before storing a managed pointer, and routes every managed-pointer assignment
through `gc_pointer_assign()`. For the copying collector, only registered roots and described
fields are updated when objects move.

```c
#include "gc.h"

void instrumented_entry(void) {
    static const gc_type_descriptor byte_type = {sizeof(unsigned char), 0, NULL};

    /* Emitted program-start and local-scope instrumentation. */
    gc_init();
    gc_scope_token scope = gc_scope_begin();
    unsigned char *slot = NULL;
    gc_scope_add_root(&slot);
    gc_pointer_assign(&slot, gc_alloc_array(&byte_type, 32));

    /* An instrumentation/runtime collection point. */
    gc_collect();

    gc_scope_end(scope);
    gc_cleanup();
}
```

The C ABI does not expose C++ exceptions. Allocation failures, unexpected internal failures, and
checked contract violations print a diagnostic and abort. Pointer and storage lifetime requirements
remain obligations of the compiler or instrumentation that emits these calls.

## Inspection API

Include `gc_debug.h` for optional statistics and memory snapshots. Its declarations and symbols do
not depend on build-system macros. Call `gc_debug_is_available()` to discover whether the linked
collector was built with `GC_ENABLE_INSPECTION`; disabled builds return `GC_DEBUG_UNAVAILABLE` from
inspection operations.

`gc_debug_get_stats()` and `gc_debug_snapshot_memory_layout()` return status codes instead of
terminating for unavailable inspection, an inactive runtime, invalid output arguments, or snapshot
allocation failures. Except for the availability query and layout disposal, inspection operations
require an initialized runtime.

Statistics use explicit units and cumulative event counts. `reclaimed_block_count` reports blocks
found unreachable and reclaimed since `gc_init()`, while `relocated_block_count` reports blocks
moved by a copying collection. Nonmoving collectors therefore report zero relocations.

```c
#include "gc_debug.h"

gc_debug_stats stats;
if (gc_debug_get_stats(&stats) == GC_DEBUG_OK) {
    /* Use stats.heap_capacity, stats.free_bytes, and the collector counters. */
}
```

A successful memory snapshot contains an explicit `block_count`; it has no sentinel entry. Release
it with `gc_debug_memory_layout_dispose()`, which clears the snapshot and accepts null. Physical
layout semantics depend on the collector: mark-and-sweep and copying report allocated and free
regions, while reference counting reports only its live, noncontiguous allocations.

## Formatting

The project uses the root `.clang-format` file for C, C++, and headers:

```sh
cmake --build --preset default --target format-check
cmake --build --preset default --target format
```

Run all repository-level formatting and English-language checks with:

```sh
cmake --build --preset default --target quality-check
```
