#include "../test_check.h"
#include "gc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct pointer_table_leaf {
    int value;
};

/* The scalar fields force padding around both pointer fields. */
struct padded_pointer_element {
    unsigned char prefix;
    struct pointer_table_leaf *first;
    uint32_t marker;
    struct pointer_table_leaf *second;
    unsigned char suffix;
};

enum { POINTER_TABLE_ARRAY_LEN = 3 };

static const size_t pointer_field_offsets[] = {
    offsetof(struct padded_pointer_element, first),
    offsetof(struct padded_pointer_element, second),
};

_Static_assert(offsetof(struct padded_pointer_element, first) <
                   offsetof(struct padded_pointer_element, second),
               "pointer fields must be emitted in canonical order");
_Static_assert(offsetof(struct padded_pointer_element, first) % _Alignof(void *) == 0,
               "first pointer field must be naturally aligned");
_Static_assert(offsetof(struct padded_pointer_element, second) % _Alignof(void *) == 0,
               "second pointer field must be naturally aligned");
_Static_assert(POINTER_TABLE_ARRAY_LEN * sizeof(struct padded_pointer_element) ==
                   sizeof(struct padded_pointer_element[POINTER_TABLE_ARRAY_LEN]),
               "array payload size must match element count and sizeof");

static void populate_leaf(struct pointer_table_leaf **temporary, int value) {
    gc_pointer_assign(temporary, gc_malloc(sizeof(struct pointer_table_leaf)));
    TEST_CHECK(*temporary != NULL);
    (*temporary)->value = value;
}

static void check_array(const struct padded_pointer_element *array) {
    for (size_t i = 0; i < POINTER_TABLE_ARRAY_LEN; ++i) {
        TEST_CHECK(array[i].prefix == (unsigned char)(0xa0u + i));
        TEST_CHECK(array[i].marker == (uint32_t)(0x100u + i));
        TEST_CHECK(array[i].suffix == (unsigned char)(0xf0u + i));
        TEST_CHECK(array[i].first != NULL);
        TEST_CHECK(array[i].second != NULL);
        TEST_CHECK(array[i].first->value == (int)(10 + i));
        TEST_CHECK(array[i].second->value == (int)(20 + i));
    }
}

int main(void) {
    TEST_CHECK(pointer_field_offsets[0] < pointer_field_offsets[1]);
    TEST_CHECK(pointer_field_offsets[0] % _Alignof(void *) == 0);
    TEST_CHECK(pointer_field_offsets[1] % _Alignof(void *) == 0);

    gc_ptr_table *table = gc_ptr_table_create(
        POINTER_TABLE_ARRAY_LEN, sizeof(struct padded_pointer_element), 2, pointer_field_offsets);
    TEST_CHECK(table != NULL);

    gc_init();
    gc_scope_token scope = gc_scope_begin();
    struct padded_pointer_element *array;
    struct pointer_table_leaf *temporary;
    gc_scope_add_root(&array);
    gc_scope_add_root(&temporary);

    gc_pointer_assign(&array,
                      gc_malloc(sizeof(struct padded_pointer_element) * POINTER_TABLE_ARRAY_LEN));
    TEST_CHECK(array != NULL);
    gc_register_object(array, table);

    for (size_t i = 0; i < POINTER_TABLE_ARRAY_LEN; ++i) {
        array[i].prefix = (unsigned char)(0xa0u + i);
        array[i].marker = (uint32_t)(0x100u + i);
        array[i].suffix = (unsigned char)(0xf0u + i);
        populate_leaf(&temporary, (int)(10 + i));
        gc_pointer_assign(&array[i].first, temporary);
        gc_pointer_assign(&temporary, NULL);
        populate_leaf(&temporary, (int)(20 + i));
        gc_pointer_assign(&array[i].second, temporary);
        gc_pointer_assign(&temporary, NULL);
    }

    for (size_t round = 0; round < 4; ++round) {
        gc_collect();
        check_array(array);
    }

    for (size_t i = 0; i < POINTER_TABLE_ARRAY_LEN; ++i) {
        gc_pointer_assign(&array[i].first, NULL);
        gc_pointer_assign(&array[i].second, NULL);
    }
    gc_pointer_assign(&array, NULL);
    gc_scope_end(scope);
    gc_cleanup();
    gc_ptr_table_destroy(table);

    puts("Pointer-table array test passed!");
    return 0;
}
