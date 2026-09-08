#include "../test_check.h"
#include "gc.h"

#include <stddef.h>

struct duplicate_pointer_element {
    void *first;
    void *second;
};

int main(void) {
    const size_t pointer_field_offsets[] = {
        offsetof(struct duplicate_pointer_element, first),
        offsetof(struct duplicate_pointer_element, first),
    };
    TEST_CHECK(pointer_field_offsets[0] == pointer_field_offsets[1]);

    (void)gc_ptr_table_create(1, sizeof(struct duplicate_pointer_element), 2,
                              pointer_field_offsets);
    return 0;
}
