# Compiler-Runtime ABI v1

## Scope and ownership

The declarations in `include/gc.h` define a low-level C11 ABI for compiler-generated lowering.
They are not an application-facing memory-management API. A compiler or instrumentation pass emits
the calls described here; application source must not call them as a general-purpose allocator.
The manually written C fixtures under `test/` model the code that such a lowering pass will emit.

`include/gc_debug.h` is separate from this ABI. It is an optional inspection interface for tests
and diagnostic tools and is not required by generated programs.

The runtime exposes one process-global collector instance. A program links exactly one of the
collector implementations, and all implementations export the same `gc_*` symbols. The ABI is
single-threaded and generated code must serialize operations on the instance. C++ implementation
types, templates, containers, and exceptions never cross the C boundary.

## Supported v1 lowering

The v1 contract covers generated code for complete, statically known C object types:

- pointer-free scalar allocations;
- structs and fixed-size arrays of structs with direct managed-pointer fields;
- nested object graphs assembled through registered pointer fields; and
- local root scopes whose storage has a stable address for the scope lifetime.

The generated program may use ordinary C control flow, calls, and recursion when it emits the
corresponding scope cleanup on every edge that leaves a managed scope. Variable-length layouts,
nonlocal jumps, uninstrumented calls retaining managed addresses, and other forms listed as
unimplemented or incompatible in [the architecture policy](architecture.md) are outside v1.

## Object layout and pointer tables

For each exact pointer-bearing object type `T`, the compiler emits one canonical
`gc_ptr_table_create()` descriptor and may share it across allocations of `T`:

- `struct_size` is exactly `sizeof(T)`;
- `array_len` is exactly the number of contiguous `T` elements in the allocation;
- the requested payload size is exactly `array_len * sizeof(T)`; and
- every entry is the byte offset of a direct pointer field, obtained from `offsetof(T, field)`.

The offset sequence is strictly increasing, naturally aligned for a pointer, in bounds for one
complete field, and therefore unique. The sequence is deterministic for a given source type. The
runtime rejects malformed shape and capacity values, but it cannot infer `T` from an opaque payload
or prove that a caller supplied the source-level `sizeof` and element count. Those exact-type and
element-count properties are compiler invariants.

The compiler creates the descriptor before registering an object and keeps it alive until
`gc_cleanup()` has made every registered object unreachable. A table must not be destroyed while
the collector could still visit an object that refers to it. Pointer-free objects do not need a
pointer table; every object with managed fields must be registered before a collection can observe
it.

## Lifecycle and root lowering

The generated program-start sequence calls `gc_init()` before any lifecycle-dependent operation.
Calling it again is a restart: old allocations, roots, and scope tokens are invalidated. Generated
teardown calls `gc_cleanup()`, which is idempotent and releases all managed memory.

For each lexical region containing managed locals, lowering emits:

1. `gc_scope_begin()` on entry;
2. one `gc_scope_add_root(&slot)` for every managed local slot before its first assignment;
3. all assignments to those slots through `gc_pointer_assign()`; and
4. `gc_scope_end()` exactly once on every exit, in reverse nesting order.

Adding a root immediately clears the slot. The slot's address and storage must remain valid until
the matching scope ends. Scope cleanup therefore has to be emitted for normal fallthrough,
`return`, `break`, `continue`, and `goto` edges that leave the scope. A slot removed by
`gc_scope_end()` no longer keeps its object alive.

## Safe points and pointer stores

`gc_malloc()` and `gc_collect()` are collection safe points. A moving collector may relocate any
reachable object at either operation. Only exact object-start pointers in registered root slots or
registered object fields are discoverable and rewritten. Generated code must not retain an
unregistered managed alias across a safe point; it must reload the value from its registered slot
or field afterward.

Every managed-pointer initialization, replacement, and clear operation is emitted as
`gc_pointer_assign(destination, source)`. The destination must be a registered root slot or a field
described by its object's table. The source must be null or a currently live managed payload. A
direct C assignment to a managed pointer bypasses collector bookkeeping and is not a v1 lowering.

The compiler emits object registration before a collection can traverse the object. Allocated
payloads are zero-initialized by the ABI, so unassigned pointer fields begin as null; subsequent
field writes still use `gc_pointer_assign()`.

## Failure and collector-selection contract

The exported ABI functions are `noexcept` at the C++ boundary and never propagate C++ exceptions.
Allocation failure, invalid pointer-table shape, invalid scope order, and checked lifecycle
violations print a diagnostic to standard error and terminate the process. These checks are cheap
runtime guards; they do not replace compiler validation of source-level types, pointer provenance,
or control-flow lowering.

An instrumented program must link one and only one collector target. Linking multiple collector
implementations would define the same global ABI symbols more than once and is unsupported. The
selected collector may differ in reclamation strategy and object movement, but it must honor the
same root, layout, and pointer-assignment contract above.

This document freezes the source-level v1 lowering contract. Any incompatible ABI change requires
an explicit contract revision and regenerated instrumented code.
