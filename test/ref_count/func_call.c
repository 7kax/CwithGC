#include "gc.h"

#include <assert.h>
#include <stdio.h>

void foo() {
    const size_t alloc_size = 100;

    void *ptr, *ptr2, *ptr3;
    gc_local_var(&ptr);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);

    assert(ptr == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);

    gc_ptr_copy(&ptr, gc_malloc(alloc_size));   // block D
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size));  // block E
    gc_ptr_copy(&ptr3, gc_malloc(alloc_size));  // block F

    // 函数结束时，gc_pop会被调用，释放所有局部变量
    gc_pop();

    // 在引用计数中，当引用被释放时内存会立即回收
    // 所以此时D、E、F已经被回收
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

    gc_ptr_copy(&ptr, gc_malloc(alloc_size));   // block A
    gc_ptr_copy(&ptr2, gc_malloc(alloc_size));  // block B
    gc_ptr_copy(&ptr3, gc_malloc(alloc_size));  // block C

    assert(gc_block_collected() == 0);
    assert(gc_root_size() == 3);

    // 初始块数为0
    size_t initial_blocks = gc_block_collected();

    foo();  // block D, E, F allocated and freed here

    // 应该有3个块被回收（D、E、F）
    assert(gc_block_collected() == initial_blocks + 3);

    // 根变量仍然是3（A、B、C仍然被引用）
    assert(gc_root_size() == 3);

    // 释放主函数中的局部变量
    gc_ptr_copy(&ptr, NULL);
    assert(gc_block_collected() == initial_blocks + 4);  // +A

    gc_ptr_copy(&ptr2, NULL);
    assert(gc_block_collected() == initial_blocks + 5);  // +B

    gc_ptr_copy(&ptr3, NULL);
    assert(gc_block_collected() == initial_blocks + 6);  // +C

    assert(gc_free_size() == heap_size);  // 所有内存都应该被回收
    assert(gc_root_size() == 3);  // 根变量数量不变

    gc_pop();  // 清理main中的根变量
    assert(gc_root_size() == 0);

    gc_cleanup();

    puts("Reference counting function call test passed!");

    return 0;
}
