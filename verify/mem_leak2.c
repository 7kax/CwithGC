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
    root = malloc(sizeof(struct tree_node));
    root->data = 1;
    root->left = malloc(sizeof(struct tree_node));
    root->left->data = 2;
    root->right = malloc(sizeof(struct tree_node));
    root->right->data = 3;
}

int _main() {
    _func();
    return 0;
}

gc_ptr_table *ptr_map = NULL;
void construct_ptr_table() {
    size_t table_size;
    if (!gc_ptr_table_size(2, &table_size))
        abort();
    ptr_map = malloc(table_size);
    assert(ptr_map != NULL);
    ptr_map->array_len = 1;
    ptr_map->struct_size = sizeof(struct tree_node);
    ptr_map->num_pointers = 2;
    ptr_map->positions[0] = offsetof(struct tree_node, left);
    ptr_map->positions[1] = offsetof(struct tree_node, right);
};
void func() {
    construct_ptr_table();

    struct tree_node *root;
    gc_local_var(&root);

    gc_ptr_copy(&root, gc_malloc(sizeof(struct tree_node)));
    gc_register(root, ptr_map);
    assert(root != NULL);

    root->data = 1;
    gc_ptr_copy(&root->left, gc_malloc(sizeof(struct tree_node)));
    gc_register(root->left, ptr_map);
    assert(root->left != NULL);
    root->left->data = 2;
    gc_ptr_copy(&root->right, gc_malloc(sizeof(struct tree_node)));
    gc_register(root->right, ptr_map);
    assert(root->right != NULL);
    root->right->data = 3;

    // 这里没有释放 root 以及 root->left 和 root->right

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
