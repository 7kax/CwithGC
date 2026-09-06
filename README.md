# CwithGC

CwithGC is a small garbage-collection runtime with a C11 API and C++20 implementations of
reference-counting, copying, and mark-and-sweep collectors.

## Build and Test

Building the project requires CMake 3.20 or newer and Ninja. The checked-in CMake presets provide
the supported local validation configurations:

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Use the `strict` preset to treat compiler warnings as errors. The `sanitizers` preset additionally
enables AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset strict
cmake --build --preset strict
ctest --preset strict

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

## C API Contract

The core public interface is C11 (`include/gc.h`); the implementation is C++20. One process-global
collector instance is provided. The collector is single-threaded and not thread-safe, so callers
must serialize initialization, allocation, root management, pointer assignment, collection, and
cleanup.

Call `gc_init()` before using the runtime. Calling it again restarts the runtime and invalidates all
previous managed pointers and scope tokens. `gc_cleanup()` is safe to call repeatedly, releases all
managed memory, and invalidates every managed pointer and active root. Call `gc_init()` again before
any further runtime operation. Pointer-table creation and destruction are independent of this
lifecycle, but a table must remain alive while a registered object can be visited.

Use explicit root scopes for local pointer storage. A scope is ended in LIFO order, and each root
slot must remain at the same address until its scope ends. `gc_local_var()` clears the slot when it
registers it. Use `gc_ptr_copy()` for every managed-pointer assignment, including pointer fields
described by a registered table; direct assignment bypasses collector bookkeeping. In the copying
collector, only registered roots and fields are updated when objects move.

```c
#include "gc.h"

int main(void) {
    gc_init();

    gc_scope_token scope = gc_scope_begin();
    int *value = NULL;
    gc_local_var((void **)&value);
    gc_ptr_copy((void **)&value, gc_malloc(sizeof(*value)));
    *value = 42;

    gc_collect();
    gc_scope_end(scope);
    gc_cleanup();
    return 0;
}
```

The C ABI does not expose C++ exceptions. Allocation failures, unexpected internal failures, and
checked contract violations print a diagnostic and abort. Pointer and storage lifetime requirements
remain caller obligations.

## Inspection API

Include `gc_debug.h` for optional statistics and memory snapshots. Its declarations and symbols do
not depend on build-system macros. Call `gc_debug_is_available()` to discover whether the linked
collector was built with `GC_ENABLE_INSPECTION`; disabled builds return `GC_DEBUG_UNAVAILABLE` from
inspection operations.

`gc_debug_get_stats()` and `gc_debug_snapshot_memory_layout()` return status codes instead of
terminating for unavailable inspection, an inactive runtime, invalid output arguments, or snapshot
allocation failures. Except for the availability query and layout disposal, inspection operations
require an initialized runtime.

```c
#include "gc_debug.h"

gc_debug_stats stats;
if (gc_debug_get_stats(&stats) == GC_DEBUG_OK) {
    /* Use stats.heap_capacity, stats.free_bytes, and related counters. */
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
