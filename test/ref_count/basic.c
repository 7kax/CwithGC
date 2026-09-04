#include "gc.h"

#include <assert.h>
#include <stdio.h>

int main() {
    const size_t meta_size = gc_meta_size();
    const size_t heap_size = gc_heap_size();

    gc_init();

    // 分配3个内存块
    int *ptr1, *ptr2, *ptr3;
    gc_local_var(&ptr1);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);

    assert(ptr1 == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);

    gc_ptr_copy(&ptr1, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr2, gc_malloc(sizeof(int)));
    gc_ptr_copy(&ptr3, gc_malloc(sizeof(int)));

    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);

    *ptr1 = 42;
    *ptr2 = 43;
    *ptr3 = 44;
    assert(*ptr1 == 42);
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);

    // 查看内存使用情况
    assert(gc_free_size() == heap_size - 3 * (sizeof(int) + meta_size));
    assert(gc_block_collected() == 0);
    assert(gc_root_size() == 3);

    // 将ptr1设为NULL，应该触发垃圾回收
    gc_ptr_copy(&ptr1, NULL);
    assert(gc_block_collected() == 1);
    assert(gc_free_size() == heap_size - 2 * (sizeof(int) + meta_size));

    // 检查其余值是否完好
    assert(*ptr2 == 43);
    assert(*ptr3 == 44);
    assert(ptr1 == NULL);

    // 测试引用共享
    int *ptr4;
    gc_local_var(&ptr4);
    gc_ptr_copy(&ptr4, ptr2); // ptr4和ptr2共享同一对象

    assert(*ptr4 == 43);
    assert(ptr2 == ptr4);

    // 引用计数现在应该是2
    // 释放一个引用，对象不应被回收
    gc_ptr_copy(&ptr2, NULL);
    assert(gc_block_collected() == 1); // 仍然是1
    assert(*ptr4 == 43);

    // 释放最后一个引用，对象应被回收
    gc_ptr_copy(&ptr4, NULL);
    assert(gc_block_collected() == 2);

    // 释放最后一个对象
    gc_ptr_copy(&ptr3, NULL);
    assert(gc_block_collected() == 3);

    // 所有内存都应该被回收
    assert(gc_free_size() == heap_size);

    gc_pop();
    gc_cleanup();

    puts("Reference counting basic test passed!");

    return 0;
}
