#ifndef GC_H
#define GC_H

#ifdef __cplusplus
extern "C" {
#endif

#define GC_DEBUG
#include <stddef.h>

// Pointer table for recursive collection of structs containing pointers.
typedef struct {
    // array_len == 1 means a single struct
    // array_len > 1 means an array of structs
    // otherwise illegal
    size_t array_len;

    // size of the struct
    size_t struct_size;

    // number of pointers in the struct
    size_t num_pointers;

    // position of the pointer in the struct
    size_t positions[0];
} gc_ptr_table;

/**
 * @brief Initialize the garbage collector.
 */
void gc_init();

/**
 * @brief Allocate memory of the given size.
 *
 * @param size Size of memory to allocate.
 * @return void*
 */
void *gc_malloc(size_t size);

/**
 * @brief Add local var '*ptr' to root set
 *
 * @param ptr Address of local var
 */
void gc_local_var(void **ptr);

/**
 * @brief Add ptr_map to meta data of struct block
 *
 * @param ptr Address of the block
 * @param ptr_map Pointer map of strcut
 */
void gc_register(void *ptr, gc_ptr_table *ptr_map);

/**
 * @brief Copy the pointer from src to dst.
 *
 * @param dst Address of the destination pointer
 * @param src Source pointer
 */
void gc_ptr_copy(void **dst, void *src);

/**
 * @brief Collect garbage.
 */
void gc_collect();

/**
 * @brief Pop all local vars from the root set
 *
 * This is used when a function returns
 */
void gc_pop();

/**
 * @brief Cleanup the garbage collector.
 *
 * This function should be called before exiting the program.
 * Actually, it is not necessary to call this function, as the OS will free the
 * memory.
 */
void gc_cleanup();

/**
 * @brief Handle allocation failure.
 */
void gc_allocation_failure(); // Allocation failure

#ifdef GC_DEBUG
size_t gc_heap_size();
size_t gc_free_size();
size_t gc_block_collected();
size_t gc_meta_size();
size_t gc_root_size();

typedef struct {
    void *start;
    size_t size;
    int is_free;
} mem_block_info;
mem_block_info *gc_mem_layout();
#endif

#ifdef __cplusplus
}
#endif

#endif // GC_H