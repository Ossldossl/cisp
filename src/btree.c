// TODO: 
#include "btree.h"

cs_BTree cs_btree_init(u32 element_size)
{
    cs_BTree result;
    result.element_size = element_size;
    result.root = (cs_BTreeNode){0};
    result.root.is_leaf = true;
    return result;
}

void* cs_btree_get(cs_BTree* bt, u32 key)
{
    cs_BTreeNode* cur = &bt->root;
    while (true) {
        if (cur == null) return null;
        if (!cur->is_leaf) {
            u8 index = 255;
            // unrolled looop
            if (cur->keys[0] >= key) {
                if (cur->keys[0] == key) return advance_ptr(&cur->data, bt->element_size * 0);
                // else
                cur = cur->children[0+1];
                continue;
            } else if (cur->keys[1] >= key) {
                if (cur->keys[1] == key) return advance_ptr(&cur->data, bt->element_size * 1);
                // else
                cur = cur->children[1+1];
                continue;
            } else if (cur->keys[2] >= key) {
                if (cur->keys[2] == key) return advance_ptr(&cur->data, bt->element_size * 2);
                // else
                cur = cur->children[2+1];
                continue;
            } else if (cur->keys[3] >= key) {
                if (cur->keys[3] == key) return advance_ptr(&cur->data, bt->element_size * 3);
                // else
                cur = cur->children[3+1];
                continue;
            } 
            else {
                cur = cur->children[0];
            }
        } else {
            // cur is a leaf node
            if (cur->keys[0] == key) {
                return advance_ptr(&cur->data, bt->element_size * 0);
            } else if (cur->keys[1] == key) {
                return advance_ptr(&cur->data, bt->element_size * 1);
            } else if (cur->keys[2] == key) {
                return advance_ptr(&cur->data, bt->element_size * 2);
            } else if (cur->keys[3] == key) {
                return advance_ptr(&cur->data, bt->element_size * 3);
            } 
            else {
                // not found
                return null;
            }
        }
    }
}

static void* find_insert(cs_BTree* bt, u32 key, bool* exists)
{
    cs_BTreeNode* cur = &bt->root;
    while (true) {
        if (cur == null) return null;
        if (!cur->is_leaf) {
            u8 index = 255;
            // unrolled looop
            if (cur->keys[0] >= key) {
                if (cur->keys[0] == key) {
                    *exists = true;
                    return advance_ptr(&cur->data, bt->element_size * 0);
                }
                // else
                cur = cur->children[0+1];
                continue;
            } else if (cur->keys[1] >= key) {
                if (cur->keys[1] == key) {
                    *exists = true;
                    return advance_ptr(&cur->data, bt->element_size * 1);
                }
                // else
                cur = cur->children[1+1];
                continue;
            } else if (cur->keys[2] >= key) {
                if (cur->keys[2] == key) {
                    *exists = true;
                    return advance_ptr(&cur->data, bt->element_size * 2);
                }
                // else
                cur = cur->children[2+1];
                continue;
            } else if (cur->keys[3] >= key) {
                if (cur->keys[3] == key) {
                    *exists = true;
                    return advance_ptr(&cur->data, bt->element_size * 3);
                }
                // else
                cur = cur->children[3+1];
                continue;
            } 
            else {
                cur = cur->children[0];
                // check if node is full
                if (cur->keys[3] != 0) {
                    // TODO: btree implementation
                }
            }
        } else {
            // cur is a leaf node
            if (cur->keys[0] == key) {
                *exists = true;
                return advance_ptr(&cur->data, bt->element_size * 0);
            } else if (cur->keys[1] == key) {
                *exists = true;
                return advance_ptr(&cur->data, bt->element_size * 1);
            } else if (cur->keys[2] == key) {
                *exists = true;
                return advance_ptr(&cur->data, bt->element_size * 2);
            } else if (cur->keys[3] == key) {
                *exists = true;
                return advance_ptr(&cur->data, bt->element_size * 3);
            } 
            else {
                // not found
                *exists = false;
                return cur;
            }
        }
    }
}

void* cs_btree_set(cs_BTree* bt, u32 key)
{
    bool already_exists = false;
    void* insert_node_or_data = find_insert(bt, key, &already_exists);
    if (already_exists) {
        return insert_node_or_data;
    }
    cs_BTreeNode* node = (cs_BTreeNode*)insert_node_or_data;
}

void cs_btree_free(cs_BTree* bt);