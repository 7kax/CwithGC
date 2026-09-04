#include "gc.h"

#include <stdlib.h>

int _main() {
    int *ptr;

    // 申请内存，此时正常工作
    ptr = malloc(sizeof(int) * 10);

    // 释放内存
    free(ptr);

    // 使用 dangling pointer
    // 这里我们尝试访问已经释放的内存，这应该会导致未定义行为
    *ptr = 42; // 使用 dangling pointer

    return 0;
}

int main() {
    gc_init();

    int *ptr;
    gc_local_var(&ptr);

    // 申请内存，此时正常工作
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    // 不需要手动释放内存，gc会自动处理

    // 使用 ptr，此时 ptr 必定不是 dangling pointer
    *ptr = 42; // 使用 ptr

    gc_pop();
    gc_cleanup();

    return 0;
}