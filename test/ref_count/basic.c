#include "../test_layout.h"
#include "../test_types.h"
#include "gc.h"

#include <stdio.h>

static void assert_live_layout(const gc_debug_memory_layout *layout, void *const payloads[],
                               size_t payload_count, size_t block_size) {
    size_t entry_count = 0;
    for (size_t i = 0; i < layout->block_count; ++i) {
        const gc_debug_memory_block *entry = &layout->blocks[i];
        TEST_CHECK(entry->state == GC_DEBUG_BLOCK_ALLOCATED);
        TEST_CHECK(entry->size == block_size);

        size_t matching_payloads = 0;
        for (size_t i = 0; i < payload_count; ++i) {
            void *block_start = (unsigned char *)payloads[i] - test_gc_metadata_size();
            if (entry->start == block_start)
                ++matching_payloads;
        }
        TEST_CHECK(matching_payloads == 1);
        ++entry_count;
    }
    TEST_CHECK(entry_count == payload_count);

    for (size_t i = 0; i < payload_count; ++i) {
        void *block_start = (unsigned char *)payloads[i] - test_gc_metadata_size();
        size_t matching_entries = 0;
        for (size_t j = 0; j < layout->block_count; ++j) {
            const gc_debug_memory_block *entry = &layout->blocks[j];
            if (entry->start == block_start)
                ++matching_entries;
        }
        TEST_CHECK(matching_entries == 1);
    }
}

int main(void) {
    gc_init();

    const size_t int_block_size = test_gc_block_size(sizeof(int));
    const size_t heap_capacity = test_gc_heap_capacity();

    gc_scope_token scope = gc_scope_begin();

    // Allocate three memory blocks.
    int *ptr1, *ptr2, *ptr3;
    gc_scope_add_root(&ptr1);
    gc_scope_add_root(&ptr2);
    gc_scope_add_root(&ptr3);

    TEST_CHECK(ptr1 == NULL);
    TEST_CHECK(ptr2 == NULL);
    TEST_CHECK(ptr3 == NULL);

    gc_pointer_assign(&ptr1, gc_alloc_object(test_gc_int_type()));
    gc_pointer_assign(&ptr2, gc_alloc_object(test_gc_int_type()));
    gc_pointer_assign(&ptr3, gc_alloc_object(test_gc_int_type()));

    TEST_CHECK(ptr1 != NULL);
    TEST_CHECK(ptr2 != NULL);
    TEST_CHECK(ptr3 != NULL);

    *ptr1 = 42;
    *ptr2 = 43;
    *ptr3 = 44;
    TEST_CHECK(*ptr1 == 42);
    TEST_CHECK(*ptr2 == 43);
    TEST_CHECK(*ptr3 == 44);

    // Check memory usage.
    TEST_CHECK(test_gc_free_bytes() == heap_capacity - 3 * int_block_size);
    TEST_CHECK(test_gc_reclaimed_block_count() == 0);
    TEST_CHECK(test_gc_root_count() == 3);

    gc_debug_memory_layout layout;
    {
        void *live_payloads[] = {ptr1, ptr2, ptr3};
        layout = test_gc_memory_layout();
        assert_live_layout(&layout, live_payloads, 3, int_block_size);
        test_gc_dispose_memory_layout(&layout);
    }

    // Clear ptr1, which should trigger reclamation.
    gc_pointer_assign(&ptr1, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == 1);
    TEST_CHECK(test_gc_free_bytes() == heap_capacity - 2 * int_block_size);

    void *remaining_payloads[] = {ptr2, ptr3};
    layout = test_gc_memory_layout();
    assert_live_layout(&layout, remaining_payloads, 2, int_block_size);
    test_gc_dispose_memory_layout(&layout);

    // Verify that the remaining values are intact.
    TEST_CHECK(*ptr2 == 43);
    TEST_CHECK(*ptr3 == 44);
    TEST_CHECK(ptr1 == NULL);

    // Test shared references.
    int *ptr4;
    gc_scope_add_root(&ptr4);
    gc_pointer_assign(&ptr4, ptr2); // ptr4 and ptr2 share the same object.

    TEST_CHECK(*ptr4 == 43);
    TEST_CHECK(ptr2 == ptr4);

    // The reference count should now be two.
    // Releasing one reference must not reclaim the object.
    gc_pointer_assign(&ptr2, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == 1); // Still one.
    TEST_CHECK(*ptr4 == 43);

    // Release the final reference; the object should be reclaimed.
    gc_pointer_assign(&ptr4, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == 2);

    // Release the final object.
    gc_pointer_assign(&ptr3, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == 3);

    // All memory should have been reclaimed.
    TEST_CHECK(test_gc_free_bytes() == heap_capacity);

    gc_scope_end(scope);
    gc_cleanup();

    puts("Reference counting basic test passed!");

    return 0;
}
