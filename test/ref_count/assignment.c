#include "../test_debug.h"
#include "../test_types.h"
#include "gc.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct holder {
    int *child;
};

static const size_t holder_pointer_offsets[] = {offsetof(struct holder, child)};
static const gc_type_descriptor holder_type = {
    sizeof(struct holder),
    sizeof(holder_pointer_offsets) / sizeof(holder_pointer_offsets[0]),
    holder_pointer_offsets,
};

static void test_self_assignment(void) {
    gc_scope_token scope = gc_scope_begin();
    int *ptr;
    gc_scope_add_root(&ptr);
    gc_pointer_assign(&ptr, gc_alloc_object(test_gc_int_type()));
    *ptr = 42;

    int *original = ptr;
    const size_t free_bytes = test_gc_free_bytes();
    const size_t reclaimed_count = test_gc_reclaimed_block_count();

    gc_pointer_assign(&ptr, ptr);

    TEST_CHECK(ptr == original);
    TEST_CHECK(*ptr == 42);
    TEST_CHECK(test_gc_free_bytes() == free_bytes);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count);

    gc_pointer_assign(&ptr, NULL);
    gc_scope_end(scope);
}

static void test_child_promotion(void) {
    gc_scope_token scope = gc_scope_begin();
    struct holder *parent;
    int *temporary;
    int *promoted;
    gc_scope_add_root(&parent);
    gc_scope_add_root(&temporary);
    gc_scope_add_root(&promoted);

    gc_pointer_assign(&parent, gc_alloc_object(&holder_type));
    gc_pointer_assign(&temporary, gc_alloc_object(test_gc_int_type()));
    *temporary = 43;
    gc_pointer_assign(&parent->child, temporary);
    gc_pointer_assign(&temporary, NULL);

    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    gc_pointer_assign(&promoted, parent->child);
    gc_pointer_assign(&parent, NULL);

    TEST_CHECK(*promoted == 43);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 1);

    gc_pointer_assign(&promoted, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 2);
    gc_scope_end(scope);
}

static void test_field_replacement(void) {
    gc_scope_token scope = gc_scope_begin();
    struct holder *parent;
    int *temporary;
    int *replacement;
    gc_scope_add_root(&parent);
    gc_scope_add_root(&temporary);
    gc_scope_add_root(&replacement);

    gc_pointer_assign(&parent, gc_alloc_object(&holder_type));
    gc_pointer_assign(&temporary, gc_alloc_object(test_gc_int_type()));
    *temporary = 44;
    gc_pointer_assign(&parent->child, temporary);
    gc_pointer_assign(&temporary, NULL);

    gc_pointer_assign(&replacement, gc_alloc_object(test_gc_int_type()));
    *replacement = 45;

    const size_t reclaimed_count = test_gc_reclaimed_block_count();
    gc_pointer_assign(&parent->child, replacement);

    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 1);
    TEST_CHECK(parent->child == replacement);
    TEST_CHECK(*parent->child == 45);

    gc_pointer_assign(&replacement, NULL);
    TEST_CHECK(*parent->child == 45);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 1);

    gc_pointer_assign(&parent, NULL);
    TEST_CHECK(test_gc_reclaimed_block_count() == reclaimed_count + 3);
    gc_scope_end(scope);
}

int main(void) {
    gc_init();

    test_self_assignment();
    test_child_promotion();
    test_field_replacement();

    TEST_CHECK(test_gc_free_bytes() == test_gc_heap_capacity());
    TEST_CHECK(test_gc_root_count() == 0);

    gc_cleanup();
    puts("Reference counting assignment test passed!");
    return 0;
}
