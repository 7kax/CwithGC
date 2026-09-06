#ifndef GC_H
#define GC_H

#ifdef __cplusplus
extern "C" {
#define GC_NOEXCEPT noexcept
#else
#define GC_NOEXCEPT
#endif

#include <stddef.h>
#include <stdint.h>

/*
 * Runtime contract:
 *
 * - The process has one collector instance. It is single-threaded and is not
 *   thread-safe; callers must serialize every operation on the instance.
 * - Except for the explicitly lifecycle-independent functions below, runtime
 *   operations require a successful gc_init() and an active runtime.
 * - gc_init() may restart an existing runtime. Restarting releases the old
 *   collector state and invalidates every old managed pointer and scope token.
 * - gc_cleanup() is idempotent. It releases all managed memory and invalidates
 *   every managed pointer, root, and scope token. Call gc_init() before using
 *   the runtime again.
 *
 * C ABI failure behavior: exported functions never propagate C++ exceptions.
 * Allocation failures, unexpected internal exceptions, and checked contract
 * violations print a diagnostic to stderr and abort. Pointer and storage
 * lifetime requirements documented below are caller obligations and cannot all
 * be validated by the runtime; violating one is invalid behavior.
 */

// Opaque description of pointer fields in a struct or an array of structs.
typedef struct gc_ptr_table gc_ptr_table;

// Opaque lifetime token for one active local-root scope.
typedef uint64_t gc_scope_token;

/**
 * @brief Create an immutable pointer table.
 *
 * The positions array is copied. Every position must name a complete,
 * naturally aligned pointer field inside a struct of struct_size bytes. For an
 * array, struct_size must preserve pointer alignment between elements.
 * array_len, struct_size and num_pointers must all be greater than zero.
 * Invalid input and allocation failure terminate the process according to the
 * C ABI failure contract.
 *
 * A table may be shared by any number of registered objects. It must remain
 * alive until those objects can no longer be visited by the collector. The
 * simplest valid lifetime is to destroy tables after gc_cleanup(). This
 * function is independent of the collector lifecycle and does not require
 * gc_init().
 *
 * @param array_len Number of consecutive structs described by the table.
 * @param struct_size Size of one struct in bytes.
 * @param num_pointers Number of pointer-field offsets.
 * @param positions Pointer-field offsets, normally produced by offsetof().
 * @return Newly allocated pointer table.
 */
gc_ptr_table *gc_ptr_table_create(size_t array_len, size_t struct_size, size_t num_pointers,
                                  const size_t *positions) GC_NOEXCEPT;

/**
 * @brief Destroy a pointer table created by gc_ptr_table_create().
 *
 * Passing null is allowed. Destroying a table still referenced by a registered
 * object is invalid. This function is independent of the collector lifecycle;
 * the table must not be accessed after this call.
 */
void gc_ptr_table_destroy(gc_ptr_table *table) GC_NOEXCEPT;

/**
 * @brief Initialize or restart the garbage collector.
 *
 * A repeated call is a restart, not a no-op: all allocations and roots from
 * the previous runtime are released. Pointers into those allocations and
 * tokens returned by previous gc_scope_begin() calls become invalid. A failed
 * initialization terminates the process according to the C ABI failure
 * contract.
 */
void gc_init(void) GC_NOEXCEPT;

/**
 * @brief Begin a local-root scope.
 *
 * Every call to gc_local_var() must occur between a matching begin/end pair.
 * Scopes nest, and scope tokens must be ended in reverse order. The storage
 * for every root slot added to a scope must remain alive and at the same
 * address until that scope ends.
 *
 * @return A token identifying the newly active scope.
 */
gc_scope_token gc_scope_begin(void) GC_NOEXCEPT;

/**
 * @brief End a local-root scope.
 *
 * The token must identify the innermost active scope. Ending an unknown or
 * out-of-order token terminates the process according to the C ABI failure
 * contract. Ending a scope removes its root slots; those slots no longer keep
 * their managed objects alive.
 */
void gc_scope_end(gc_scope_token token) GC_NOEXCEPT;

/**
 * @brief Allocate a zero-initialized managed payload.
 *
 * The returned address points to the payload, not collector metadata. The
 * payload is zero-initialized for the requested number of bytes. It is owned
 * by the collector and must not be passed to free(). The allocation is not a
 * root: protect the pointer with gc_local_var() or store it in a registered
 * pointer field before another allocation or collection can occur.
 *
 * Requests that cannot be represented or do not fit in the collector heap
 * terminate the process according to the C ABI failure contract.
 *
 * @param size Number of payload bytes to allocate.
 * @return The managed payload address.
 */
void *gc_malloc(size_t size) GC_NOEXCEPT;

/**
 * @brief Add a pointer slot to the active root scope.
 *
 * ptr_address must point to writable, naturally aligned pointer storage whose
 * lifetime extends through gc_scope_end(). The slot is set to
 * null immediately, so register it before assigning a managed pointer. The
 * runtime updates registered slots when a moving collector relocates objects.
 * An active scope is required. The slot itself does not become a managed
 * allocation and must not be registered more than once for the same scope.
 *
 * @param ptr_address Address of the pointer slot to root.
 */
void gc_local_var(void *ptr_address) GC_NOEXCEPT;

/**
 * @brief Associate a pointer table with a managed payload.
 *
 * ptr must be the exact payload address returned by gc_malloc(). ptr_map must
 * be a table created by gc_ptr_table_create(), and that table must remain
 * alive for as long as the object can be visited by the collector. Register an
 * object at most once. Registration does not root the object; retain it in a
 * root slot or another registered pointer field before a collection.
 *
 * @param ptr Exact managed payload address.
 * @param ptr_map Immutable pointer table describing the payload's pointer fields.
 */
void gc_register(void *ptr, const gc_ptr_table *ptr_map) GC_NOEXCEPT;

/**
 * @brief Assign one managed pointer slot through the collector.
 *
 * dst_address must identify either a root slot previously added with
 * gc_local_var() or a pointer field inside a registered payload, and the field
 * must be described by that payload's pointer table. src must be null or a
 * pointer to a currently live managed payload (an address returned by
 * gc_malloc() or written by the collector into a registered slot). Every
 * assignment, replacement, and clearing of a managed pointer must use this
 * function; direct C assignment bypasses reference-count bookkeeping and
 * violates the collector-independent pointer-assignment contract.
 *
 * For the copying collector, only registered roots and fields are rewritten
 * when an object moves. Unregistered aliases can therefore become stale after
 * gc_collect().
 *
 * @param dst_address Address of the destination pointer slot.
 * @param src Managed payload address or null.
 */
void gc_ptr_copy(void *dst_address, void *src) GC_NOEXCEPT;

/**
 * @brief Run the collector's collection operation.
 *
 * The reference-counting collector reclaims objects as their counts reach
 * zero, so this operation does not perform a tracing pass. Unreachable
 * reference-count cycles are not reclaimed; they remain until a cycle is
 * explicitly broken or gc_cleanup() releases the runtime. The copying
 * collector evacuates reachable objects and updates registered pointer slots;
 * object addresses may change. The mark-and-sweep collector marks reachable
 * objects and reclaims unreachable ones without moving live objects.
 */
void gc_collect(void) GC_NOEXCEPT;

/**
 * @brief Release the collector runtime and all managed memory.
 *
 * This function is idempotent and does not require a prior gc_init(). It
 * releases all managed allocations and active root scopes. Every pointer into
 * a managed allocation, including pointers held in caller-owned fields, is
 * invalid after this call; do not dereference, copy, or pass one to another GC
 * API. Call gc_init() before any further runtime operation.
 */
void gc_cleanup(void) GC_NOEXCEPT;

/**
 * @brief Report an allocation failure and terminate.
 *
 * This function does not return and is normally used by collector internals.
 */
void gc_allocation_failure(void) GC_NOEXCEPT;

#ifdef GC_DEBUG
size_t gc_heap_size(void) GC_NOEXCEPT;
size_t gc_free_size(void) GC_NOEXCEPT;
size_t gc_block_collected(void) GC_NOEXCEPT;
size_t gc_meta_size(void) GC_NOEXCEPT;
size_t gc_root_size(void) GC_NOEXCEPT;

typedef struct {
    void *start;
    size_t size;
    int is_free;
} mem_block_info;
mem_block_info *gc_mem_layout(void) GC_NOEXCEPT;

/**
 * @brief Release a memory layout returned by gc_mem_layout().
 *
 * Passing null is allowed.
 */
void gc_mem_layout_free(mem_block_info *layout) GC_NOEXCEPT;
#endif

#ifdef __cplusplus
}
#endif

#undef GC_NOEXCEPT

#endif // GC_H
