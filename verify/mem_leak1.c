#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// 不安全版本
void _func() {
    int *ptr;
    ptr = malloc(sizeof(int) * 10);
}

int _main() {
    _func();
    return 0;
}

void func() {
    int *ptr;
    gc_local_var(&ptr);

    gc_ptr_copy(&ptr, gc_malloc(sizeof(int) * 10));

    gc_pop();
}

int main() {
    gc_init();

    int prev_free_size = gc_free_size();

    func();

    // 触发垃圾回收
    gc_collect();

    // 计算内存泄漏的大小
    int curr_free_size = gc_free_size();
    int leak_size = prev_free_size - curr_free_size;

    // 断言内存泄漏的大小为0
    assert(leak_size == 0);

    gc_pop();

    gc_cleanup();

    return 0;
}
