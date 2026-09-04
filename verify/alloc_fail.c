#include "gc.h"

/**
 * @brief 此文件验证算法在申请内存超过堆大小时会调用abort函数终止程序。
 *
 * 测试逻辑：
 * 1. 正常情况下，此程序应当在尝试申请过大内存时被终止
 * 2. 如果程序没有被终止（即执行了checking代码），表示测试通过
 * 3. 我们在main函数结束前调用exit(EXIT_SUCCESS)，这样CTest可以检测到正常退出
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

// 不安全版本
int _main() {
    int *ptr;

    // 申请内存，此时正常工作
    ptr = malloc(sizeof(int) * 10);

    // 申请过大内存，malloc会返回NULL
    ptr = malloc(__LONG_LONG_MAX__);

    return 0;
}

// 信号处理函数，用于捕获abort导致的信号
void signal_handler(int sig) {
    if (sig == SIGABRT) {
        exit(EXIT_SUCCESS); // 成功退出，表示测试通过
    }
}

int main() {
    // 注册信号处理函数，捕获SIGABRT信号
    signal(SIGABRT, signal_handler);

    gc_init();

    // 获取当前堆的大小
    size_t heap_size = gc_heap_size();

    // 获取当前可用空间大小
    size_t free_size = gc_free_size();

    // 定义一个指针用于持有分配的内存
    void *ptr;
    gc_local_var(&ptr);

    // 先分配一部分内存，确保垃圾收集器工作正常
    ptr = gc_malloc(free_size / 4);
    assert(ptr != NULL);

    // 触发一次垃圾收集
    gc_collect();

    // 更新可用空间大小
    free_size = gc_free_size();

    // 尝试分配超过可用空间的内存
    // 这里我们尝试分配比当前可用空间大两倍的内存，这应该会导致分配失败
    // 这次分配应该会失败并调用std::abort
    ptr = gc_malloc(free_size * 2);

    // 如果程序继续执行到这里，表示库没有在分配失败时调用abort

    // 清理资源
    gc_pop();
    gc_cleanup();

    // 如果执行到这里，意味着测试失败
    return EXIT_FAILURE;
}