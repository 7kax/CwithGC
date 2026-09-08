# Compiler-Runtime ABI v2

## Scope and ownership

The declarations in `include/gc.h` define a low-level C11 ABI for compiler-generated lowering.
They are not an application-facing memory-management API. A compiler or instrumentation pass emits
the calls described here; application source must not call them as a general-purpose allocator.
The manually written C fixtures under `test/` model the code that such a lowering pass will emit.

`include/gc_debug.h` is separate from this ABI. It is an optional inspection interface for tests
and diagnostic tools and is not required by generated programs.

The runtime exposes one process-global collector instance. A program links exactly one collector,
and all collector implementations export the same `gc_*` symbols. The ABI is single-threaded and
generated code must serialize operations on the instance. C++ implementation types, containers,
and exceptions never cross the C boundary.

## Supported v2 lowering

The v2 contract covers generated code for complete C object types whose alignment does not exceed
`_Alignof(max_align_t)`:

- pointer-free scalar and aggregate objects;
- individual structures and arrays whose length is known at allocation time;
- structures and fixed-size subarrays containing managed pointer fields;
- nested object graphs assembled through described pointer fields; and
- local root scopes whose storage has a stable address for the scope lifetime.

Variable-length arrays as local objects, flexible array members, nonlocal jumps, and other forms
listed as unimplemented or incompatible in [the architecture policy](architecture.md) remain
outside v2. A runtime array allocation may nevertheless receive a dynamically computed element
count because its element layout is still statically known.

## Static type descriptors

For every allocated element type `T`, the compiler emits one immutable `gc_type_descriptor` and may
share it across all object and array allocations of `T`:

- `element_size` is exactly `sizeof(T)` and is greater than zero;
- `pointer_count` is the number of managed pointer fields recursively contained in one `T`;
- `pointer_offsets` points to a static array of that many byte offsets, or is null when the count is
  zero; and
- each offset identifies a complete, naturally aligned managed pointer field within `T`.

Offsets include managed fields nested inside structures and fixed-size subarrays. They are emitted
in strictly increasing order and are unique. The descriptor and its offset array remain valid until
`gc_cleanup()` has made all corresponding allocations unreachable. Static storage duration is the
canonical lowering.

The runtime rejects a null descriptor, zero element size, or a missing nonempty offset array. It
does not rediscover source types or validate individual compiler-generated offsets. Exact sizes,
canonical offsets, alignment, and descriptor lifetime are compiler invariants.

The generic slot ABI carries pointer values as opaque object representations. A v2 target must
provide the same size and representation for every managed object-pointer type and `void *`; the
compiler must reject target/type combinations that do not satisfy this requirement. The runtime
uses byte-wise loads and stores so it does not alias a typed pointer object as `void **`. Function
pointers are not managed object pointers.

## Typed allocation

Lower one standalone `T` allocation to `gc_alloc_object(&descriptor_for_T)`. Lower an allocation of
`length` consecutive `T` elements to `gc_alloc_array(&descriptor_for_T, length)`.

The array length is an allocation property rather than a type property. The runtime stores it in
the allocation header and scans each element at:

```text
payload + element_index * element_size
```

It then visits every `pointer_offsets[pointer_index]` relative to that element. A single-object
allocation uses an element count of one. A zero-length array, a multiplication overflow, or a
request larger than the collector capacity reaches the allocation-failure path.

Allocation atomically installs the descriptor and element count before returning. Payload bytes are
zeroed, and each described slot is additionally written with the target's null pointer
representation. There is no separate object-registration phase and no untyped byte-allocation ABI.

A managed `T *` may denote a standalone `T` or the first element of an allocated `T` array. Only
the allocation base may persist in a root or managed field. Interior and one-past pointers may be
used as transient derived values but must not escape or survive a safe point.

## Lifecycle and root lowering

The generated program-start sequence calls `gc_init()` before any lifecycle-dependent operation.
Calling it again is a restart: old allocations, roots, and scope tokens are invalidated. Generated
teardown calls `gc_cleanup()`, which is idempotent and releases all managed memory.

For each lexical region containing managed locals, lowering emits:

1. `gc_scope_begin()` on entry;
2. one `gc_scope_add_root(&slot)` for every managed local before its first assignment;
3. all assignments to those slots through `gc_pointer_assign()`; and
4. `gc_scope_end()` exactly once on every exit, in reverse nesting order.

Adding a root immediately clears the slot. Its address and storage remain valid until the matching
scope ends. Scope cleanup must therefore be emitted for normal fallthrough, `return`, `break`,
`continue`, and `goto` edges that leave the scope.

## Safe points and pointer stores

`gc_alloc_object()`, `gc_alloc_array()`, and `gc_collect()` are collection safe points. A moving
collector may relocate any reachable allocation at these operations. Only allocation-base pointers
in registered root slots or described object fields are discovered and rewritten. Generated code
must reload managed values after a safe point instead of retaining unregistered aliases.

When a destination slot is inside a managed object, lowering first evaluates every allocation or
other safe-point expression, then reloads the host object and forms the destination address. It
must not combine a potentially moving allocation with a previously formed field address.

Every managed-pointer initialization, replacement, and clear is emitted as
`gc_pointer_assign(destination, source)`. The destination is a registered root or a field named by
the allocation's descriptor. The source is null or a live allocation base. Direct C assignment to
a managed pointer bypasses collector bookkeeping and is not valid v2 lowering.

## Failure and collector selection

Exported ABI functions are `noexcept` at the C++ boundary and never propagate C++ exceptions.
Allocation failure, a structurally unusable descriptor, invalid scope order, and checked lifecycle
violations print a diagnostic to standard error and terminate the process. These guards do not
replace compiler validation of source types, pointer provenance, or control-flow lowering.

An instrumented program links one and only one collector target. Collectors may differ in
reclamation strategy and object movement, but they honor the same root, descriptor, typed
allocation, and pointer-assignment contract.

This document freezes the source-level v2 lowering contract. Any incompatible ABI change requires
an explicit contract revision and regenerated instrumented code.
