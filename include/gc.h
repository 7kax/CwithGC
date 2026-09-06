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
 * C ABI contract: exported functions never propagate C++ exceptions. Internal
 * allocation failures and unexpected C++ exceptions print an error and abort.
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
 * simplest valid lifetime is to destroy tables after gc_cleanup().
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
 * object is invalid.
 */
void gc_ptr_table_destroy(gc_ptr_table *table) GC_NOEXCEPT;

/**
 * @brief Initialize the garbage collector.
 */
void gc_init(void) GC_NOEXCEPT;

/**
 * @brief Begin a local-root scope.
 *
 * Every call to gc_local_var() must occur between a matching begin/end pair.
 * Scope tokens must be ended in reverse order.
 *
 * @return A token identifying the newly active scope.
 */
gc_scope_token gc_scope_begin(void) GC_NOEXCEPT;

/**
 * @brief End a local-root scope.
 *
 * The token must identify the innermost active scope. Ending an unknown or
 * out-of-order token terminates the process according to the C ABI failure
 * contract.
 */
void gc_scope_end(gc_scope_token token) GC_NOEXCEPT;

/**
 * @brief Allocate memory of the given size.
 *
 * @param size Size of memory to allocate.
 * @return void*
 */
void *gc_malloc(size_t size) GC_NOEXCEPT;

/**
 * @brief Add local var '*ptr' to root set
 *
 * @param ptr_address Address of the local pointer variable
 */
void gc_local_var(void *ptr_address) GC_NOEXCEPT;

/**
 * @brief Add ptr_map to meta data of struct block
 *
 * @param ptr Address of the block
 * @param ptr_map Pointer map of struct
 */
void gc_register(void *ptr, const gc_ptr_table *ptr_map) GC_NOEXCEPT;

/**
 * @brief Copy the pointer from src to dst.
 *
 * @param dst_address Address of the destination pointer
 * @param src Source pointer
 */
void gc_ptr_copy(void *dst_address, void *src) GC_NOEXCEPT;

/**
 * @brief Collect garbage.
 */
void gc_collect(void) GC_NOEXCEPT;

/**
 * @brief Pop all local vars from the root set
 *
 * This is used when a function returns
 */
void gc_pop(void) GC_NOEXCEPT;

/**
 * @brief Cleanup the garbage collector.
 *
 * This function should be called before exiting the program.
 * Actually, it is not necessary to call this function, as the OS will free the
 * memory.
 */
void gc_cleanup(void) GC_NOEXCEPT;

/**
 * @brief Handle allocation failure.
 */
void gc_allocation_failure(void) GC_NOEXCEPT; // Allocation failure

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
