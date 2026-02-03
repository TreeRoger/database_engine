#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stdbool.h>

// B-tree node structure
#define BTREE_ORDER 4  // Minimum degree (t). Node can have 2t-1 to 2t keys

typedef struct btree_node {
    bool is_leaf;
    uint32_t num_keys;
    char **keys;
    char **values;
    struct btree_node **children;
    struct btree_node *parent;
} btree_node_t;

typedef struct {
    btree_node_t *root;
    uint32_t height;
} btree_t;

// Initialize a new B-tree
btree_t* btree_create(void);

// Free B-tree and all nodes
void btree_destroy(btree_t *tree);

// Search for a key in the B-tree
char* btree_search(btree_t *tree, const char *key);

// Insert a key-value pair
int btree_insert(btree_t *tree, const char *key, const char *value);

// Delete a key
int btree_delete(btree_t *tree, const char *key);

// Print the B-tree (for debugging)
void btree_print(btree_t *tree);

// Iterate over all key-value pairs in order; callback receives (key, value, ctx)
void btree_foreach(btree_t *tree, void (*cb)(const char *key, const char *value, void *ctx), void *ctx);

#endif // BTREE_H
