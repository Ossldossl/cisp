#pragma once
#include <stdlib.h>
#include "common.h"

typedef struct cs_BTree cs_BTree;
typedef struct cs_BTreeNode cs_BTreeNode;

#define CS_BTREE_KEYCOUNT 4
#define CS_BTREE_CHILDCOUNT 5

struct cs_BTreeNode {
    u32 keys[CS_BTREE_KEYCOUNT];
    cs_BTreeNode* children[CS_BTREE_CHILDCOUNT];
    bool is_leaf;
    u8 data[];
};

struct cs_BTree {
    u32 element_size;
    cs_BTreeNode root;
};

cs_BTree cs_btree_init(u32 element_size);
void* cs_btree_set(cs_BTree* bt, u32 key);
void* cs_btree_get(cs_BTree* bt, u32 key);
void cs_btree_free(cs_BTree* bt);