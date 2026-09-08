#ifndef CWITHGC_GC_H
#define CWITHGC_GC_H

#ifdef __cplusplus
extern "C" {
#define CWITHGC_NOEXCEPT noexcept
#else
#define CWITHGC_NOEXCEPT
#endif

#include <stddef.h>
#include <stdint.h>

/*
 * Compiler/instrumentation ABI v2 contract:
 *
 * This is a low-level ABI for compiler-generated calls and compiler-inserted
 * instrumentation. It is not a general-purpose application allocation API.
 * A language runtime or instrumentation pass lowers managed allocations,
 * pointer updates, and local-root lifetimes to these operations.
 * ABI v2 requires every managed object-pointer type used in a slot to have the
 * same size and representation as void *. Function pointers are not managed
 * object pointers. The compiler must reject targets that do not provide this
 * representation invariant.
 *
 * - The process has one collector instance. It is single-threaded and is not
 *   thread-safe; generated code and runtime integration must serialize every
 *   operation on the instance.
 * - Except for the explicitly lifecycle-independent functions below, runtime
 *   operations require a successful gc_init() and an active runtime.
 * - gc_init() may restart an existing runtime. Restarting releases the old
 *   collector state and invalidates every old managed pointer and scope token.
 * - gc_cleanup() is idempotent. It releases all managed memory and invalidates
 *   every managed pointer, root, and scope token. Instrumented program teardown
 *   must complete before the runtime is initialized again.
 *
 * C ABI failure behavior: exported functions never propagate C++ exceptions.
 * Allocation failures, unexpected internal exceptions, and checked contract
 * violations print a diagnostic to stderr and abort. Pointer and storage
 * lifetime requirements documented below are instrumentation obligations and
 * cannot all be validated by the runtime; violating one is invalid behavior.
 */

/**
 * @brief Immutable compiler-generated description of one complete element type.
 *
 * element_size is sizeof(T). pointer_offsets contains pointer_count canonical,
 * strictly increasing byte offsets of managed pointer fields within T. A
 * pointer-free type has pointer_count equal to zero and pointer_offsets equal
 * to null. The descriptor and its offset array must have static storage
 * duration because managed allocations retain the descriptor address.
 */
typedef struct {
    size_t element_size;
    size_t pointer_count;
    const size_t *pointer_offsets;
} gc_type_descriptor;

// Opaque lifetime token for one active local-root scope.
typedef uint64_t gc_scope_token;

/**
 * @brief Initialize or restart the garbage-collection runtime.
 *
 * The generated program-start sequence must invoke this before any other
 * lifecycle-dependent ABI operation. A repeated call is a restart, not a
 * no-op: all allocations and roots from the previous runtime are released.
 * Pointers into those allocations and tokens returned by previous
 * gc_scope_begin() calls become invalid. A failed initialization terminates
 * the process according to the C ABI failure contract.
 */
void gc_init(void) CWITHGC_NOEXCEPT;

/**
 * @brief Begin a compiler-instrumented local-root scope.
 *
 * Instrumentation must emit gc_scope_add_root() calls between a matching
 * begin/end pair. Scopes nest, and scope tokens must be ended in reverse
 * order. The storage for every root slot added to a scope must remain alive
 * and at the same address until that scope ends.
 *
 * @return A token identifying the newly active scope.
 */
gc_scope_token gc_scope_begin(void) CWITHGC_NOEXCEPT;

/**
 * @brief End a compiler-instrumented local-root scope.
 *
 * The token must identify the innermost active scope. Ending an unknown or
 * out-of-order token terminates the process according to the C ABI failure
 * contract. Ending a scope removes its root slots; those slots no longer keep
 * their managed objects alive.
 */
void gc_scope_end(gc_scope_token token) CWITHGC_NOEXCEPT;

/**
 * @brief Allocate one zero-initialized managed object.
 *
 * type must point to valid, immutable compiler-generated metadata with static
 * storage duration. Allocation and layout installation are atomic from the
 * collector's perspective. The returned payload is aligned for
 * _Alignof(max_align_t), its non-pointer bytes are zeroed, and every described
 * managed pointer field contains the target's null pointer representation.
 * Over-aligned managed types are outside ABI v2.
 *
 * The returned address is owned by the collector and must not be passed to
 * free(). Generated code must root or store it before another safe point.
 *
 * @param type Static descriptor for the complete allocated type.
 * @return The managed payload address.
 */
void *gc_alloc_object(const gc_type_descriptor *type) CWITHGC_NOEXCEPT;

/**
 * @brief Allocate a zero-initialized managed array.
 *
 * element_type describes one complete array element. length is stored in the
 * allocation metadata and used to traverse every element. It must be greater
 * than zero, and length times element_size must be representable as size_t and
 * fit in the selected collector heap. The returned managed pointer denotes the
 * first element; persistent interior and one-past pointers are outside ABI v2.
 * All other ownership, alignment, initialization, and safe-point rules are the
 * same as for gc_alloc_object().
 *
 * @param element_type Static descriptor for one complete array element.
 * @param length Number of elements to allocate.
 * @return The managed array base address.
 */
void *gc_alloc_array(const gc_type_descriptor *element_type, size_t length) CWITHGC_NOEXCEPT;

/**
 * @brief Register an instrumented local pointer slot with the active scope.
 *
 * root_slot must point to writable, naturally aligned pointer storage whose
 * lifetime extends through gc_scope_end(). The slot is set to null immediately,
 * so instrumentation must register it before assigning a managed pointer. The
 * runtime treats the slot representation as opaque and updates it when a moving
 * collector relocates objects.
 * An active scope is required. The slot itself does not become a managed
 * allocation and must not be registered more than once for the same scope.
 *
 * @param root_slot Address of the pointer slot to root.
 */
void gc_scope_add_root(void *root_slot) CWITHGC_NOEXCEPT;

/**
 * @brief Record an instrumented managed-pointer assignment.
 *
 * destination_slot must identify either a root slot previously added with
 * gc_scope_add_root() or a pointer field described by its allocation's type
 * descriptor. source must be null or a pointer to a currently live managed
 * payload returned by a typed allocation operation or written by the
 * collector. Every assignment, replacement, and clearing of a managed pointer
 * must use this function; direct assignment by generated code bypasses
 * reference-count bookkeeping and violates the collector-independent
 * pointer-assignment contract.
 *
 * Allocation and collection are safe points for the copying collector. Only
 * registered roots and described fields are rewritten when an object moves.
 * Generated code must not keep an unregistered managed alias across either
 * safe point; it must reload the value from a registered root or described
 * field afterward. If the destination is a field, lowering must allocate first
 * and form its address only after reloading the host object; do not emit
 * gc_pointer_assign(&object->field, gc_alloc_object(type)).
 *
 * @param destination_slot Address of the destination pointer slot.
 * @param source Managed payload address or null.
 */
void gc_pointer_assign(void *destination_slot, void *source) CWITHGC_NOEXCEPT;

/**
 * @brief Run the collector's operation requested by instrumented code.
 *
 * The reference-counting collector reclaims objects as their counts reach
 * zero, so this operation does not perform a tracing pass. Unreachable
 * reference-count cycles are not reclaimed; they remain until a cycle is
 * explicitly broken or gc_cleanup() releases the runtime. The copying
 * collector evacuates reachable objects and updates registered pointer slots;
 * object addresses may change. The mark-and-sweep collector marks reachable
 * objects and reclaims unreachable ones without moving live objects.
 */
void gc_collect(void) CWITHGC_NOEXCEPT;

/**
 * @brief Release the runtime during instrumented program teardown.
 *
 * This function is idempotent and does not require a prior gc_init(). It
 * releases all managed allocations and active root scopes. Every pointer into
 * a managed allocation, including pointers held in instrumented program
 * fields, is invalid after this call; generated code must not dereference,
 * copy, or pass one to another GC ABI operation. A subsequent generated
 * startup sequence must invoke gc_init() before any further runtime operation.
 */
void gc_cleanup(void) CWITHGC_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#undef CWITHGC_NOEXCEPT

#endif // CWITHGC_GC_H
