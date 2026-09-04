#include "gc.h"

#include <assert.h>
#include <stdlib.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

// 不安全版本
void _func() {
    struct tree_node *root;
    root = malloc(sizeof(struct tree_node) * 10);

    for (int i = 0; i < 10; i++) {
        root[i].data = i;
        root[i].left = malloc(sizeof(struct tree_node));
        root[i].left->data = i + 1;
        root[i].right = malloc(sizeof(struct tree_node));
        root[i].right->data = i + 2;
    }
}

int _main() {
    _func();
    return 0;
}

gc_ptr_table *ptr_map = NULL;
gc_ptr_table *ptr_map_array = NULL;
void construct_ptr_table_array() {
    ptr_map_array = malloc(sizeof(gc_ptr_table) + 2 * sizeof(size_t));
    ptr_map_array->array_len = 10;
    ptr_map_array->struct_size = sizeof(struct tree_node);
    ptr_map_array->num_pointers = 2;
    ptr_map_array->positions[0] = offsetof(struct tree_node, left);
    ptr_map_array->positions[1] = offsetof(struct tree_node, right);
};
void construct_ptr_table() {
    ptr_map = malloc(sizeof(gc_ptr_table) + 2 * sizeof(size_t));
    ptr_map->array_len = 1;
    ptr_map->struct_size = sizeof(struct tree_node);
    ptr_map->num_pointers = 2;
    ptr_map->positions[0] = offsetof(struct tree_node, left);
    ptr_map->positions[1] = offsetof(struct tree_node, right);
};

void func() {
    construct_ptr_table_array();
    construct_ptr_table();

    struct tree_node *root;
    gc_local_var(&root);

    gc_ptr_copy(&root, gc_malloc(sizeof(struct tree_node) * 10));
    // root 指向一个结构体数组
    gc_register(root, ptr_map_array);
    assert(root != NULL);

    for (int i = 0; i < 10; i++) {
        root[i].data = i;
        gc_ptr_copy(&root[i].left, gc_malloc(sizeof(struct tree_node)));
        // root[i].left 指向一个结构体
        gc_register(root[i].left, ptr_map);
        assert(root[i].left != NULL);
        root[i].left->data = i + 1;
        gc_ptr_copy(&root[i].right, gc_malloc(sizeof(struct tree_node)));
        // root[i].right 指向一个结构体
        gc_register(root[i].right, ptr_map);
        assert(root[i].right != NULL);
        root[i].right->data = i + 2;
    }

    gc_pop();
};

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