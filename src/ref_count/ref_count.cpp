#include "../gc_layout.h"
#include "../gc_ptr_table_internal.h"
#include "../gc_runtime.h"
#include "gc.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

const size_t heap_size = 4 * 1024; // 堆大小 4KB

// 栈指针结构体
struct stack_ptr {
    void **ptr;  // 局部变量指针
    void *frame; // 包含指针的栈帧的地址
};
std::vector<stack_ptr> root; // 局部变量指针列表

size_t free_size; // 可用内存大小

// 元数据结构体
struct meta_data {
    const gc_ptr_table *ptr_table; // 指向指针表的指针
    size_t ref_count;              // 引用计数
    size_t size;                   // 块大小
};

#ifdef GC_DEBUG
size_t block_collected = 0; // 已回收的块数量
#endif

static meta_data *get_meta_data(void *ptr) {
    return gc_layout::metadata<meta_data>(ptr);
}

[[noreturn]] static void invalid_pointer_table() {
    gc_runtime::fatal("Invalid pointer table");
}

// 增加引用计数
static void increment_ref_count(void *ptr) {
    assert(ptr != nullptr);

    meta_data *meta_ptr = get_meta_data(ptr);
    meta_ptr->ref_count++;
}

// 减少引用计数，如果计数为0则释放内存
static void decrement_ref_count(void *ptr) {
    assert(ptr != nullptr);

    meta_data *meta_ptr = get_meta_data(ptr);

    if (meta_ptr->ref_count > 0) {
        meta_ptr->ref_count--;
    }

    if (meta_ptr->ref_count == 0) {
        // 减少子对象的引用计数
        if (meta_ptr->ptr_table != nullptr) {
            u_int64_t cur_struct = (u_int64_t)ptr;
            for (size_t i = 0; i < meta_ptr->ptr_table->array_len; i++) {
                for (size_t j = 0; j < meta_ptr->ptr_table->positions.size(); j++) {
                    void **child_ptr = (void **)(cur_struct + meta_ptr->ptr_table->positions[j]);
                    if (*child_ptr != nullptr) {
                        decrement_ref_count(*child_ptr);
                    }
                }
                cur_struct += meta_ptr->ptr_table->struct_size;
            }
        }

        // Read all required metadata before releasing the allocation.
        const size_t released_size = meta_ptr->size;
        free_size += released_size;

#ifdef GC_DEBUG
        block_collected++;
#endif

        std::free(meta_ptr);
    }
}

extern "C" {
void gc_init(void) noexcept try {
    free_size = heap_size;

#ifdef GC_DEBUG
    block_collected = 0;
#endif
} catch (...) {
    gc_runtime::handle_current_exception();
}

void *gc_malloc(size_t size) noexcept try {
    size_t alloc_size;
    if (!gc_layout::block_size<meta_data>(size, alloc_size) || alloc_size > heap_size)
        gc_allocation_failure();

    if (alloc_size > free_size)
        gc_allocation_failure();

    void *block = std::malloc(alloc_size);
    if (block == nullptr)
        gc_allocation_failure();
    free_size -= alloc_size;

    // 初始化元数据
    meta_data *meta_ptr = (meta_data *)block;
    meta_ptr->ptr_table = nullptr; // 初始化指针表
    meta_ptr->ref_count = 0;       // 初始化引用计数
    meta_ptr->size = alloc_size;   // 设置块大小

    // 清空数据块
    void *payload = gc_layout::payload(meta_ptr);
    std::memset(payload, 0, size);

    return payload;
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_local_var(void *ptr_address) noexcept try {
    auto **ptr = static_cast<void **>(ptr_address);
    void *frame_address = __builtin_frame_address(1);
    root.push_back({ptr, frame_address});

    // 清除指针
    *ptr = nullptr;
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_register(void *ptr, const gc_ptr_table *ptr_map) noexcept try {
    meta_data *meta_ptr = get_meta_data(ptr);

    // 只允许注册一次
    assert(meta_ptr->ptr_table == nullptr);

    // 保证 ptr_map 合法
    if (ptr_map == nullptr)
        invalid_pointer_table();
    assert(ptr_map->array_len > 0);
    assert(ptr_map->struct_size > 0);
    assert(!ptr_map->positions.empty());

    size_t payload_size;
    const size_t payload_capacity = meta_ptr->size - gc_layout::header_size<meta_data>;
    if (!gc_layout::checked_mul(ptr_map->array_len, ptr_map->struct_size, payload_size) ||
        payload_size > payload_capacity)
        invalid_pointer_table();

    meta_ptr->ptr_table = ptr_map;
} catch (...) {
    gc_runtime::handle_current_exception();
}

void gc_allocation_failure(void) noexcept {
    gc_runtime::fatal("Allocation failure");
}

void gc_collect(void) noexcept {
    // 在引用计数法中，垃圾收集是实时的
    // 当引用计数为0时，对象会被立即回收
    // 此函数保留为空实现，以兼容接口
}

void gc_cleanup(void) noexcept {
    free_size = 0;
    root.clear();
}

void gc_ptr_copy(void *dst_address, void *src) noexcept {
    auto **dst = static_cast<void **>(dst_address);
    void *old = *dst;

    if (old == src) {
        return;
    }

    // Retain the new object before releasing the old one. The old object may
    // own src and recursively release it when its reference count reaches 0.
    if (src != nullptr) {
        increment_ref_count(src);
    }

    // Store before releasing old because dst may be a field inside old.
    *dst = src;

    if (old != nullptr) {
        decrement_ref_count(old);
    }
}

void gc_pop(void) noexcept {
    void *frame_address = root.back().frame;

    // 弹出当前栈帧中的所有局部变量，并减少它们指向对象的引用计数
    while (!root.empty() && root.back().frame == frame_address) {
        void **ptr = root.back().ptr;
        if (*ptr != nullptr) {
            decrement_ref_count(*ptr);
        }
        root.pop_back();
    }
}

#ifdef GC_DEBUG
size_t gc_heap_size(void) noexcept {
    return heap_size;
}

size_t gc_free_size(void) noexcept {
    return free_size;
}

size_t gc_block_collected(void) noexcept {
    return block_collected;
}

size_t gc_meta_size(void) noexcept {
    return gc_layout::header_size<meta_data>;
}

size_t gc_root_size(void) noexcept {
    return root.size();
}

mem_block_info *gc_mem_layout(void) noexcept {
    return nullptr;
}
#endif
}
