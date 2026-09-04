#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

gc_ptr_table *ptr_table = NULL;

gc_ptr_table *construct_ptr_table() {
    gc_ptr_table *table = malloc(sizeof(gc_ptr_table) + 7 * sizeof(size_t));

    table->array_len = 1;
    table->struct_size = sizeof(struct foo);
    table->num_pointers = 7;
    table->positions[0] = offsetof(struct foo, b);
    table->positions[1] = offsetof(struct foo, c);
    table->positions[2] = offsetof(struct foo, d);
    table->positions[3] = offsetof(struct foo, e);
    table->positions[4] = offsetof(struct foo, f);
    table->positions[5] = offsetof(struct foo, g);
    table->positions[6] = offsetof(struct foo, h);
    return table;
}

int main() {
    ptr_table = construct_ptr_table();

    gc_init();

    const size_t meta_size = gc_meta_size();

    struct foo *ptr;
    gc_local_var(&ptr);
    gc_ptr_copy(&ptr, gc_malloc(sizeof(struct foo)));

    assert(ptr->a == 0);
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

    // 分配并设置递归引用
    gc_ptr_copy(&ptr->b, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->c, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr->d, gc_malloc(sizeof(int)));
    assert(gc_block_collected() == 0);
    assert(gc_root_size() == 1);

    ptr->a = 666;
    *ptr->b = 42;
    *ptr->c = 43;
    *ptr->d = 44;

    // 检查值是否正确设置
    assert(ptr->a == 666);
    assert(*ptr->b == 42);
    assert(*ptr->c == 43);
    assert(*ptr->d == 44);

    // 清除引用，这应该触发内存回收
    gc_ptr_copy(&ptr->b, NULL);
    gc_ptr_copy(&ptr->c, NULL);
    gc_ptr_copy(&ptr->d, NULL);

    // 引用计数垃圾收集是即时的，所以当引用被清除时应该已经回收了
    assert(gc_block_collected() == 3); // 3个整数对象

    // 将根对象设为NULL并进行垃圾收集
    gc_ptr_copy(&ptr, NULL);
    assert(gc_block_collected() == 4); // 3个整数对象 + 1个结构体

    // 内存应该全部被回收
    assert(gc_free_size() == gc_heap_size());

    gc_pop();
    gc_cleanup();

    puts("Reference counting recursion test passed!");

    return 0;
}
