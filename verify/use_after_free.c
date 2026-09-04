#include "gc.h"

#include <stdlib.h>

int _main() {
    int *ptr;

    // 申请内存，此时正常工作
    ptr = malloc(sizeof(int) * 10);

    // 另一个指针指向同一块内存
    int *ptr2 = ptr;

    // 释放 ptr
    free(ptr);

    // 使用 ptr2 访问内存
    // 这里我们尝试访问已经释放的内存，这应该会导致未定义行为
    *ptr2 = 42;

    return 0;
}

int main() {
    gc_init();

    int *ptr;
    gc_local_var(&ptr);

    // 申请内存，此时正常工作
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    int *ptr2;
    gc_local_var(&ptr2);

    // 另一个指针指向同一块内存
    gc_ptr_copy(&ptr2, ptr);

    // 无需手动释放内存，gc会自动处理

    // 使用 ptr2，此时安全
    *ptr2 = 42; // 使用 ptr2

    gc_pop();
    gc_cleanup();

    return 0;
}