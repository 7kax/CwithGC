#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

struct foo {
    int *a;
    int *b;
};

// 不安全版本
int _main() {
    // 局部指针变量（模拟未初始化的状态）
    int *ptr1 = (int *)(uintptr_t)0xff;
    int *ptr2 = (int *)(uintptr_t)0xff;
    int *ptr3 = (int *)(uintptr_t)0xff;

    assert(ptr1 == NULL); // 断言失败
    assert(ptr2 == NULL); // 断言失败
    assert(ptr3 == NULL); // 断言失败

    // 局部结构体的指针成员（模拟未初始化的状态）
    struct foo f = {(int *)(uintptr_t)0xff, (int *)(uintptr_t)0xff};

    assert(f.a == NULL); // 断言失败
    assert(f.b == NULL); // 断言失败

    return 0;
}

int main() {
    gc_init();

    // 局部指针变量（模拟未初始化的状态）
    int *ptr1 = (int *)(uintptr_t)0xff;
    int *ptr2 = (int *)(uintptr_t)0xff;
    int *ptr3 = (int *)(uintptr_t)0xff;
    gc_local_var(&ptr1);
    gc_local_var(&ptr2);
    gc_local_var(&ptr3);

    assert(ptr1 == NULL);
    assert(ptr2 == NULL);
    assert(ptr3 == NULL);

    // 局部结构体的指针成员（模拟未初始化的状态）
    struct foo f = {(int *)(uintptr_t)0xff, (int *)(uintptr_t)0xff};
    gc_local_var(&f.a);
    gc_local_var(&f.b);

    assert(f.a == NULL);
    assert(f.b == NULL);

    gc_pop();

    gc_cleanup();

    return 0;
}
