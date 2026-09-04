#include "gc.h"

#include <stdlib.h>

int _main() {
    int *ptr;

    // 申请内存，此时正常工作
    ptr = malloc(sizeof(int) * 10);

    // 释放内存
    free(ptr);

    // 再次释放内存
    free(ptr);

    return 0;
}

int main() {
    gc_init();

    int *ptr;
    gc_local_var(&ptr);

    // 申请内存，此时正常工作
    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    // 无需手动释放内存，gc会自动处理

    gc_pop();
    gc_cleanup();

    return 0;
}