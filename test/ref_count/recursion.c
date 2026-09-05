#include "../test_layout.h"
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

gc_ptr_table *construct_ptr_table(void) {
    const size_t positions[] = {
        offsetof(struct foo, b), offsetof(struct foo, c), offsetof(struct foo, d),
        offsetof(struct foo, e), offsetof(struct foo, f), offsetof(struct foo, g),
        offsetof(struct foo, h),
    };
    gc_ptr_table *table = gc_ptr_table_create(1, sizeof(struct foo), 7, positions);
    assert(table != NULL);
    return table;
}

int main() {
    ptr_table = construct_ptr_table();

    gc_init();

    const size_t struct_block_size = test_gc_block_size(sizeof(struct foo));

    struct foo *ptr;
    gc_local_var(&ptr);
    gc_ptr_copy(&ptr, gc_malloc(sizeof(struct foo)));
    gc_register(ptr, ptr_table);

    assert(ptr->a == 0);
    assert(ptr->b == NULL);
    assert(ptr->c == NULL);
    assert(ptr->d == NULL);
    assert(ptr->e == NULL);
    assert(ptr->f == NULL);
    assert(ptr->g == NULL);
    assert(ptr->h == NULL);

    assert(gc_free_size() == gc_heap_size() - struct_block_size);
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
    gc_ptr_table_destroy(ptr_table);

    puts("Reference counting recursion test passed!");

    return 0;
}
