#include "gc.h"

#include <assert.h>
#include <stdio.h>

struct foo {
    int a;
    int *b;
    int *c;
    int *d;
    int *e;
    int *f;
    int *g;
    int *h;
};

long long ptr_table = {0x7f00000000000000};

int main() {
    gc_init();

    const size_t meta_size = gc_meta_size();

    struct foo *ptr;
    gc_local_var(&ptr);
    gc_ptr_copy(&ptr, gc_malloc(sizeof(struct foo)));
    ptr->a = 666;
    ptr->b = &ptr->a;
    ptr->c = &ptr->a;
    gc_register(ptr, &ptr_table);
    assert(ptr->a == 666);
    assert(ptr->b == NULL);
    assert(ptr->c == NULL);
    assert(ptr->d == NULL);
    assert(ptr->e == NULL);
    assert(ptr->f == NULL);
    assert(ptr->g == NULL);
    assert(ptr->h == NULL);
    assert(gc_free_size() == gc_heap_size() - sizeof(struct foo) - meta_size);
    assert(gc_root_size() == 1);
    assert(gc_block_collected() == 0);

    gc_ptr_copy(&ptr->b, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->c, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->d, gc_malloc(sizeof(int)));
    assert(gc_block_collected() == 0);
    assert(gc_root_size() == 1);
    assert(gc_free_size() == gc_heap_size() - 3 * sizeof(int) - sizeof(struct foo) - 4 * meta_size);

    gc_ptr_copy((void **)&ptr, NULL);
    gc_collect();
    assert(gc_block_collected() == 4);
    assert(gc_free_size() == gc_heap_size());

    gc_pop();

    gc_cleanup();

    puts("Mark-sweep recursion test passed!");

    return 0;
}