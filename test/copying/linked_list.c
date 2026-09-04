#include "gc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct node {
    int data;
    struct node *next;
};

gc_ptr_table *construct_ptr_table() {
    gc_ptr_table *table = malloc(sizeof(gc_ptr_table) + 1 * sizeof(size_t));

    table->array_len = 1;
    table->struct_size = sizeof(struct node);
    table->num_pointers = 1;
    table->positions[0] = offsetof(struct node, next);
    return table;
}

gc_ptr_table *ptr_map = NULL;

struct node *make_node(int data) {
    struct node *new_node;
    gc_local_var(&new_node);

    gc_ptr_copy(&new_node, gc_malloc(sizeof(struct node)));
    gc_register(new_node, ptr_map);

    new_node->data = data;

    gc_pop();

    return new_node;
}

int main() {
    int elements[] = { 1, 2, 3, 4, 5 };
    int n = 5;
    ptr_map = construct_ptr_table();

    gc_init();

    struct node *head, *cur, *new_node;
    gc_local_var(&head);
    gc_local_var(&cur);
    gc_local_var(&new_node);

    gc_ptr_copy(&head, make_node(elements[0]));
    gc_ptr_copy(&cur, head);

    // 创建一个链表
    for (int i = 1; i < n; i++) {
        gc_ptr_copy(&new_node, make_node(elements[i]));
        gc_ptr_copy(&(cur->next), new_node);
        gc_ptr_copy(&cur, new_node);
    }

    // 记录第一个节点地址
    struct node *pre_head = head;

    // 触发垃圾收集
    gc_collect();
    assert(gc_block_collected() == n);

    // 地址应该发生变化
    assert(head != pre_head);

    // 清理辅助指针
    gc_ptr_copy(&cur, NULL);
    gc_ptr_copy(&new_node, NULL);

    // 遍历链表检查值
    for (int i = 0; i < n; i++) {
        assert(head->data == elements[i]);
        gc_ptr_copy((void **)&head, head->next);
    }

    assert(head == NULL);
    assert(cur == NULL);
    assert(new_node == NULL);

    // 此时所有节点应该都被回收
    gc_collect();
    assert(gc_free_size() == gc_heap_size());

    gc_pop();
    gc_cleanup();
    puts("Copying linked list test passed!");

    return 0;
}
