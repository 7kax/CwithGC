#include "gc.h"

#include <assert.h>
#include <stdio.h>

void foo() {
    const size_t alloc_size = 100;
    const size_t meta_size = gc_meta_size();
    const size_t heap_size = gc_heap_size();

    void *ptr, *ptr2, *ptr3;
    gc_local_var(&ptr);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);
    assert(ptr == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);
    gc_ptr_copy(&ptr, gc_malloc(alloc_size)); // block D
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size)); // block E
    gc_ptr_copy(&ptr3, gc_malloc(alloc_size)); // block F

    gc_pop();
}

int main() {
    gc_init();

    const size_t alloc_size = 100;
    const size_t meta_size = gc_meta_size();
    const size_t heap_size = gc_heap_size();

    void *ptr, *ptr2, *ptr3;
    gc_local_var(&ptr);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);
    assert(ptr == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);
    gc_ptr_copy(&ptr, gc_malloc(alloc_size)); // block A
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size)); // block B
    gc_ptr_copy(&ptr3, gc_malloc(alloc_size)); // block C

    // 记录初始地址以验证复制后的地址变化
    void *pre_ptr = ptr;
    void *pre_ptr2 = ptr2;
    void *pre_ptr3 = ptr3;

    foo(); // block D, E, F allocated here
    assert(gc_root_size() == 3);

    gc_collect();

    // 复制垃圾收集器会改变对象地址
    assert(ptr != pre_ptr);
    assert(ptr2 != pre_ptr2);
    assert(ptr3 != pre_ptr3);

    assert(gc_block_collected() == 3); // 只剩下 A, B, C

    gc_pop();
    assert(gc_root_size() == 0);

    gc_cleanup();

    puts("Copying function call test passed!");

    return 0;
}
