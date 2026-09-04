#include "gc.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

void *from;                        // Start of the from space
void *to;                          // Start of the to space
const size_t heap_size = 4 * 1024; // 4KB

void *free_space; // Start of the free space in from space
size_t free_size; // Size of the free space in from space

struct stack_ptr {
  void **ptr;
  void *frame;
};
std::vector<stack_ptr> root;

struct meta_data {
  u_int8_t copied;
  gc_ptr_table *ptr_table;
  size_t size;
  void *forwarding;
  u_int8_t data[0];
};

#ifdef GC_DEBUG
size_t block_collected = 0;
#endif

static meta_data *get_meta_data(void *ptr) {
  return (meta_data *)((u_int64_t)ptr - sizeof(meta_data));
}

static void *copy(void *ptr) {
  meta_data *meta_ptr = get_meta_data(ptr);

  if (meta_ptr->copied) {
    assert(meta_ptr->forwarding != nullptr);
    return meta_ptr->forwarding;
  }

  // Copy the block
  std::memcpy(free_space, meta_ptr, meta_ptr->size);

#ifdef GC_DEBUG
  block_collected++;
#endif

  // Set the copy info
  meta_ptr->copied = 1;
  meta_ptr->forwarding = (void *)((u_int64_t)free_space + sizeof(meta_data));

  // Update the free pointer
  free_space = (void *)((u_int64_t)free_space + meta_ptr->size);

  if (meta_ptr->ptr_table != nullptr) {
    u_int64_t cur_struct = (u_int64_t)ptr;
    for (int i = 0; i < meta_ptr->ptr_table->array_len; i++) {
      for (int j = 0; j < meta_ptr->ptr_table->num_pointers; j++) {
        void **child_ptr =
            (void **)(cur_struct + meta_ptr->ptr_table->positions[j]);
        if (*child_ptr != nullptr) {
          // Copy the child pointer
          *child_ptr = copy(*child_ptr);
        }
      }
      cur_struct += meta_ptr->ptr_table->struct_size;
    }
  }

  return meta_ptr->forwarding;
}

extern "C" {
void gc_init() {
  from = std::malloc(heap_size);
  to = std::malloc(heap_size);
  free_space = from;
  free_size = heap_size;

#ifdef GC_DEBUG
  block_collected = 0;
#endif
}

void *gc_malloc(size_t size) {
  size_t alloc_size = size + sizeof(meta_data);

  // If the allocation size is larger than the free size, collect garbage
  if (alloc_size > free_size)
    gc_collect();

  // If the allocation size is still larger than the free size, fail
  if (alloc_size > free_size)
    gc_allocation_failure();

  // Allocate the block
  meta_data *block = (meta_data *)free_space;
  free_space = (char *)free_space + alloc_size;
  free_size -= alloc_size;

  // Set the meta data
  block->copied = 0;
  block->forwarding = nullptr;
  block->ptr_table = nullptr;
  block->size = alloc_size;

  // Clear the block
  std::memset(block->data, 0, size);

  return &block->data;
}

void gc_local_var(void **ptr) {
  void *frame_address = __builtin_frame_address(1);
  root.push_back({ptr, frame_address});

  *ptr = nullptr; // Clear the pointer
}

void gc_register(void *ptr, gc_ptr_table *ptr_map) {
  meta_data *meta_ptr = get_meta_data(ptr);

  // 只允许注册一次
  assert(meta_ptr->ptr_table == nullptr);

  // 保证 ptr_map 合法
  assert(ptr_map != nullptr);
  assert(ptr_map->array_len > 0);
  assert(ptr_map->struct_size > 0);
  assert(ptr_map->num_pointers > 0);

  meta_ptr->ptr_table = ptr_map;
}

// No need to handle this in copying
void gc_ptr_copy(void **dst, void *src) { *dst = src; }

void gc_collect() {
  free_space = to;
  for (auto [ptr_address, frame] : root)
    if (*ptr_address != nullptr)
      *ptr_address = copy(*ptr_address);

  // Swap the spaces
  std::swap(from, to);

  // Reset free size
  free_size = heap_size - ((char *)free_space - (char *)from);
}

void gc_pop() {
  void *frame_address = root.back().frame;
  while (!root.empty() && root.back().frame == frame_address) {
    root.pop_back();
  }
}

void gc_cleanup() {
  std::free(from);
  std::free(to);
  from = nullptr;
  to = nullptr;
  free_space = nullptr;
  free_size = 0;
  root.clear();
}

void gc_allocation_failure() {
  std::cerr << "Allocation failure" << std::endl;
  std::abort();
}

#ifdef GC_DEBUG
size_t gc_heap_size() { return heap_size; }
size_t gc_free_size() { return free_size; }
size_t gc_block_collected() { return block_collected; }
size_t gc_meta_size() { return sizeof(meta_data); }
size_t gc_root_size() { return root.size(); }
mem_block_info *gc_mem_layout() {
  std::vector<mem_block_info> mem_layout;

  void *current = from;
  while (current < free_space) {
    mem_block_info block;
    block.start = current;
    block.size = ((meta_data *)current)->size;
    block.is_free = 0;
    mem_layout.push_back(block);
    current = (char *)current + block.size;
  }

  mem_layout.push_back({free_space, free_size, 1});

  mem_block_info *layout = new mem_block_info[mem_layout.size() + 1];
  std::copy(mem_layout.begin(), mem_layout.end(), layout);
  layout[mem_layout.size()] = {nullptr, 0, 0}; // Null-terminate the array

  return layout;
}
#endif
}