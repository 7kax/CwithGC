# CwithGC Architecture

## Runtime Boundaries

CwithGC uses a C11 ABI with C++20 collector implementations. `include/gc.h` is the boundary between
compiler-generated instrumentation and the selected GC runtime; it is not a manually called
application allocation API. `include/gc_debug.h` is a separate, stable inspection interface for
tests and diagnostic tools.

The complete source-level lowering contract is specified in [Compiler-Runtime ABI v2](abi.md).
This document records the architecture and resulting feature-support policy; the ABI document
records the call ordering and compiler obligations that generated code must satisfy.

The compiler owns the lowering rules and metadata described by `gc.h`. Generated code performs
runtime startup and teardown, root-scope management, typed allocation, managed-pointer assignment,
and collection calls. Exactly one collector implementation is linked into a program. C++
implementation types, exceptions, templates, and standard-library containers must not cross either
C ABI.

## Managed Pointer and Object Model

- `gc_type_descriptor` metadata is immutable compiler-generated data for one complete element type.
  It stores the element size and recursively flattened managed-pointer offsets, but not an array
  length.
- Every managed pointer names either a standalone object whose concrete type matches its static
  pointee type or the first element of an array with that element type. Managed pointers cannot
  erase or reinterpret the element layout through incompatible casts or type punning.
- Type descriptors contain canonical, strictly increasing and unique field offsets. Dynamic array
  length is stored per allocation and must be greater than zero.
- Every managed-pointer store is visible to instrumentation. Typed allocation installs the object
  layout atomically; generated code registers roots before a collection can observe them.
- A managed allocation may move at any allocation or collection safe point. Only allocation-base
  pointers in registered or described slots are rewritten.

These restrictions deliberately trade some C flexibility for precise object traversal, reliable
reference-count updates, and pointer rewriting by the copying collector.

## C Feature Support Policy

### Compatible but Not Yet Implemented

These features can be supported with compiler analysis, lowering, or a focused runtime extension:

| Feature | Required support |
| --- | --- |
| Local, global, and static managed pointers | Emit roots with the correct storage lifetime and initialize program-lifetime roots during startup |
| Function parameters, return values, recursion, and typed function pointers | Define temporary-root ownership across calls and preserve roots at every possible safe point |
| Early return, `break`, `continue`, and `goto` | Insert balanced scope teardown on every control-flow edge that exits a managed scope |
| Structs, fixed-size arrays, dynamic-length allocations, and nested object graphs | Generate one canonical descriptor for each element type, flatten nested managed-pointer offsets, and select object or array allocation lowering |
| Struct assignment and known `memcpy` / `memmove` operations containing managed fields | Lower pointer fields through the write barrier while copying non-pointer bytes normally |
| Variable-length arrays and flexible array members | Define stack-root and trailing-storage lowering beyond the existing heap-array element descriptor |
| `setjmp` / `longjmp` | Add compiler-generated root-stack restoration so nonlocal control flow cannot leave stale scopes registered |
| Calls into uninstrumented libraries | Introduce explicit wrappers, handles, or pinning before a raw managed address crosses the boundary |

### Incompatible with the Selected Precise-GC Design

These patterns violate exact-layout, exact-root, or moving-collector requirements. The compiler must
diagnose them when they involve managed data:

| Feature | Design conflict |
| --- | --- |
| Interior or one-past pointers that escape or survive a safe point | The runtime rewrites exact object-start pointers only and cannot reconstruct arbitrary derived addresses after relocation |
| Incompatible pointer casts, source-level managed `void *` type erasure, and type punning | Compiler-generated pointer metadata would no longer describe the concrete object reached through the pointer |
| Managed pointers encoded as integers or stored in untyped byte buffers across a safe point | The value is invisible to precise root discovery and relocation |
| General unions whose active alternatives have different managed-pointer layouts | A static type descriptor does not provide reliable active-member information |
| Pointer-field writes or bytewise copies hidden from instrumentation | Reference counting misses ownership changes and moving collectors cannot update hidden slots |
| External code retaining raw managed addresses across a safe point | The collector cannot discover or rewrite aliases outside registered storage |
| Managed object-pointer types with a representation different from `void *` | The untyped v2 slot ABI cannot perform a type-aware conversion; a future typed-slot ABI is required |
| Over-aligned managed types requiring more than `_Alignof(max_align_t)` | The fixed heap layout provides fundamental alignment only; a future aligned-allocation ABI is required |
