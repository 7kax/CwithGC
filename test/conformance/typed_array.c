#include "../test_check.h"
#include "gc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct array_leaf {
    int value;
};

struct nested_pointers {
    struct array_leaf *items[2];
};

/* Scalar fields force padding around direct and nested pointer fields. */
struct array_element {
    unsigned char prefix;
    struct array_leaf *first;
    uint32_t marker;
    struct nested_pointers nested;
    struct array_leaf *second;
    unsigned char suffix;
};

enum { ARRAY_LENGTH = 3 };

static const gc_type_descriptor array_leaf_type = {sizeof(struct array_leaf), 0, NULL};
static const size_t array_element_pointer_offsets[] = {
    offsetof(struct array_element, first),
    offsetof(struct array_element, nested) + offsetof(struct nested_pointers, items[0]),
    offsetof(struct array_element, nested) + offsetof(struct nested_pointers, items[1]),
    offsetof(struct array_element, second),
};
static const gc_type_descriptor array_element_type = {
    sizeof(struct array_element),
    sizeof(array_element_pointer_offsets) / sizeof(array_element_pointer_offsets[0]),
    array_element_pointer_offsets,
};

_Static_assert(offsetof(struct array_element, first) <
                   offsetof(struct array_element, nested) +
                       offsetof(struct nested_pointers, items[0]),
               "pointer fields must be emitted in canonical order");
_Static_assert(offsetof(struct array_element, nested) + offsetof(struct nested_pointers, items[1]) <
                   offsetof(struct array_element, second),
               "nested pointer fields must be emitted in canonical order");
_Static_assert(sizeof(struct array_leaf *) == sizeof(void *),
               "managed object pointers must match the ABI representation size");
_Static_assert(_Alignof(struct array_leaf *) == _Alignof(void *),
               "managed object pointers must match the ABI alignment");

static void populate_leaf(struct array_leaf **temporary, int value) {
    gc_pointer_assign(temporary, gc_alloc_object(&array_leaf_type));
    TEST_CHECK(*temporary != NULL);
    (*temporary)->value = value;
}

static void check_array(const struct array_element *array) {
    for (size_t i = 0; i < ARRAY_LENGTH; ++i) {
        TEST_CHECK(array[i].prefix == (unsigned char)(0xa0u + i));
        TEST_CHECK(array[i].marker == (uint32_t)(0x100u + i));
        TEST_CHECK(array[i].suffix == (unsigned char)(0xf0u + i));
        TEST_CHECK(array[i].first->value == (int)(10 + i));
        TEST_CHECK(array[i].nested.items[0]->value == (int)(20 + i));
        TEST_CHECK(array[i].nested.items[1]->value == (int)(30 + i));
        TEST_CHECK(array[i].second->value == (int)(40 + i));
    }
}

int main(void) {
    const size_t dynamic_length = ARRAY_LENGTH;

    gc_init();
    gc_scope_token scope = gc_scope_begin();
    struct array_element *array;
    struct array_leaf *temporary;
    gc_scope_add_root(&array);
    gc_scope_add_root(&temporary);

    gc_pointer_assign(&array, gc_alloc_array(&array_element_type, dynamic_length));
    TEST_CHECK(array != NULL);

    for (size_t i = 0; i < dynamic_length; ++i) {
        TEST_CHECK(array[i].first == NULL);
        TEST_CHECK(array[i].nested.items[0] == NULL);
        TEST_CHECK(array[i].nested.items[1] == NULL);
        TEST_CHECK(array[i].second == NULL);

        array[i].prefix = (unsigned char)(0xa0u + i);
        array[i].marker = (uint32_t)(0x100u + i);
        array[i].suffix = (unsigned char)(0xf0u + i);

        populate_leaf(&temporary, (int)(10 + i));
        gc_pointer_assign(&array[i].first, temporary);
        populate_leaf(&temporary, (int)(20 + i));
        gc_pointer_assign(&array[i].nested.items[0], temporary);
        populate_leaf(&temporary, (int)(30 + i));
        gc_pointer_assign(&array[i].nested.items[1], temporary);
        populate_leaf(&temporary, (int)(40 + i));
        gc_pointer_assign(&array[i].second, temporary);
        gc_pointer_assign(&temporary, NULL);
    }

    for (size_t round = 0; round < 4; ++round) {
        gc_collect();
        check_array(array);
    }

    for (size_t i = 0; i < dynamic_length; ++i) {
        gc_pointer_assign(&array[i].first, NULL);
        gc_pointer_assign(&array[i].nested.items[0], NULL);
        gc_pointer_assign(&array[i].nested.items[1], NULL);
        gc_pointer_assign(&array[i].second, NULL);
    }
    gc_pointer_assign(&array, NULL);
    gc_scope_end(scope);
    gc_cleanup();

    puts("Typed array test passed!");
    return 0;
}
