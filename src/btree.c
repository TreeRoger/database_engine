#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btree.h"

btree_t* btree_create(void) {
    btree_t *tree = calloc(1, sizeof(btree_t));
    if (!tree) {
        return NULL;
    }
    
    // Create root node
    tree->root = calloc(1, sizeof(btree_node_t));
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    
    tree->root->is_leaf = true;
    tree->root->num_keys = 0;
    tree->root->keys = calloc(2 * BTREE_ORDER - 1, sizeof(char*));
    tree->root->values = calloc(2 * BTREE_ORDER - 1, sizeof(char*));
    tree->root->children = calloc(2 * BTREE_ORDER, sizeof(btree_node_t*));
    tree->height = 1;
    
    return tree;
}

// Recursive helper to free a node and all its children
static void btree_destroy_node(btree_node_t *node) {
    if (!node) {
        return;
    }
    
    // If not a leaf, recursively free all children
    if (!node->is_leaf && node->children) {
        for (uint32_t i = 0; i <= node->num_keys; i++) {
            if (node->children[i]) {
                btree_destroy_node(node->children[i]);
            }
        }
    }
    
    // Free all keys and values
    if (node->keys) {
        for (uint32_t i = 0; i < node->num_keys; i++) {
            if (node->keys[i]) {
                free(node->keys[i]);
            }
        }
        free(node->keys);
    }
    
    if (node->values) {
        for (uint32_t i = 0; i < node->num_keys; i++) {
            if (node->values[i]) {
                free(node->values[i]);
            }
        }
        free(node->values);
    }
    
    // Free children array
    if (node->children) {
        free(node->children);
    }
    
    // Free the node itself
    free(node);
}

// Free B-tree and all nodes, deallocating all memory
void btree_destroy(btree_t *tree) {
    if (!tree) {
        return;
    }
    
    // Recursively destroy all nodes starting from root
    if (tree->root) {
        btree_destroy_node(tree->root);
    }
    
    // Free the tree structure
    free(tree);
}

// Helper function to search for a key within a node
// Returns the index where the key is found, or the index of the child
// to search next if the key is not in this node
static uint32_t btree_search_node_index(btree_node_t *node, const char *key) {
    uint32_t i = 0;
    
    // Linear search through keys to find the position
    // In a production system, you might use binary search for better performance
    while (i < node->num_keys && strcmp(key, node->keys[i]) > 0) {
        i++;
    }
    
    return i;
}

// Recursive helper function to search for a key starting from a given node
static char* btree_search_recursive(btree_node_t *node, const char *key) {
    if (!node || !key) {
        return NULL;
    }
    
    // Find the position where the key should be (or is) in this node
    uint32_t i = btree_search_node_index(node, key);
    
    // Check if we found the key in this node
    if (i < node->num_keys && strcmp(key, node->keys[i]) == 0) {
        // Key found! Return a copy of the value
        // Caller is responsible for freeing this memory
        if (node->values[i]) {
            return strdup(node->values[i]);
        }
        return NULL;
    }
    
    // If this is a leaf node and we didn't find the key, it doesn't exist
    if (node->is_leaf) {
        return NULL;
    }
    
    // Recurse into the appropriate child node
    // Child at index i contains keys less than node->keys[i]
    // (or all keys if i == num_keys)
    if (node->children && node->children[i]) {
        return btree_search_recursive(node->children[i], key);
    }
    
    return NULL;
}

// Search for a key in the B-tree
// Returns a newly allocated string containing the value, or NULL if not found
// Caller is responsible for freeing the returned string
char* btree_search(btree_t *tree, const char *key) {
    if (!tree || !tree->root || !key) {
        return NULL;
    }
    
    // Start search from the root node
    return btree_search_recursive(tree->root, key);
}

// Create a new B-tree node with proper initialization
static btree_node_t* btree_create_node(bool is_leaf) {
    btree_node_t *node = calloc(1, sizeof(btree_node_t));
    if (!node) {
        return NULL;
    }
    
    node->is_leaf = is_leaf;
    node->num_keys = 0;
    node->keys = calloc(2 * BTREE_ORDER - 1, sizeof(char*));
    node->values = calloc(2 * BTREE_ORDER - 1, sizeof(char*));
    node->children = calloc(2 * BTREE_ORDER, sizeof(btree_node_t*));
    node->parent = NULL;
    
    if (!node->keys || !node->values || !node->children) {
        if (node->keys) free(node->keys);
        if (node->values) free(node->values);
        if (node->children) free(node->children);
        free(node);
        return NULL;
    }
    
    return node;
}

// Split a full child node at the given index
// The parent node must have space for one more key
// This function splits the child and promotes the middle key to the parent
static void btree_split_child(btree_node_t *parent, uint32_t child_index) {
    btree_node_t *child = parent->children[child_index];
    
    // Create a new node that will hold the right half of the split
    btree_node_t *new_node = btree_create_node(child->is_leaf);
    if (!new_node) {
        return;  // Memory allocation failed
    }
    
    // The new node will have BTREE_ORDER - 1 keys (right half)
    new_node->num_keys = BTREE_ORDER - 1;
    
    // Copy the right half of keys and values from child to new_node
    for (uint32_t j = 0; j < BTREE_ORDER - 1; j++) {
        new_node->keys[j] = child->keys[j + BTREE_ORDER];
        new_node->values[j] = child->values[j + BTREE_ORDER];
        child->keys[j + BTREE_ORDER] = NULL;
        child->values[j + BTREE_ORDER] = NULL;
    }
    
    // If child is not a leaf, copy the right half of children pointers
    if (!child->is_leaf) {
        for (uint32_t j = 0; j < BTREE_ORDER; j++) {
            new_node->children[j] = child->children[j + BTREE_ORDER];
            child->children[j + BTREE_ORDER] = NULL;
            if (new_node->children[j]) {
                new_node->children[j]->parent = new_node;
            }
        }
    }
    
    // Reduce the number of keys in the original child
    child->num_keys = BTREE_ORDER - 1;
    
    // Shift parent's children to the right to make room for new_node
    for (uint32_t j = parent->num_keys + 1; j > child_index + 1; j--) {
        parent->children[j] = parent->children[j - 1];
    }
    
    // Link new_node as a child of parent
    parent->children[child_index + 1] = new_node;
    new_node->parent = parent;
    
    // Shift parent's keys and values to the right
    for (uint32_t j = parent->num_keys; j > child_index; j--) {
        parent->keys[j] = parent->keys[j - 1];
        parent->values[j] = parent->values[j - 1];
    }
    
    // The middle key of the child moves up to the parent
    // This is the key at index BTREE_ORDER - 1 in the original child
    parent->keys[child_index] = child->keys[BTREE_ORDER - 1];
    parent->values[child_index] = child->values[BTREE_ORDER - 1];
    child->keys[BTREE_ORDER - 1] = NULL;
    child->values[BTREE_ORDER - 1] = NULL;
    
    parent->num_keys++;
}

// Insert a key-value pair into a non-full leaf node
// This helper assumes the node is a leaf and has space
// Returns 0 on success, -1 on error
static int btree_insert_into_leaf(btree_node_t *node, const char *key, const char *value) {
    if (!node || !key || !value || !node->is_leaf) {
        return -1;
    }
    
    // Find the position where this key should be inserted
    uint32_t i = btree_search_node_index(node, key);
    
    // Check if key already exists
    if (i < node->num_keys && strcmp(key, node->keys[i]) == 0) {
        // Key exists, update the value
        free(node->values[i]);
        node->values[i] = strdup(value);
        return 0;
    }
    
    // Shift keys and values to the right to make room for new entry
    for (uint32_t j = node->num_keys; j > i; j--) {
        node->keys[j] = node->keys[j - 1];
        node->values[j] = node->values[j - 1];
    }
    
    // Insert the new key-value pair
    node->keys[i] = strdup(key);
    node->values[i] = strdup(value);
    node->num_keys++;
    
    return 0;
}

// Recursive helper to insert into a non-full node
// This function ensures the node is not full before inserting
// Returns 0 on success, -1 on error
static int btree_insert_non_full(btree_node_t *node, const char *key, const char *value) {
    if (!node || !key || !value) {
        return -1;
    }
    
    // Find the position where this key should be
    uint32_t i = btree_search_node_index(node, key);
    
    // Check if key already exists in this node
    if (i < node->num_keys && strcmp(key, node->keys[i]) == 0) {
        // Key exists, update the value
        free(node->values[i]);
        node->values[i] = strdup(value);
        return 0;
    }
    
    // If this is a leaf node, insert directly
    if (node->is_leaf) {
        return btree_insert_into_leaf(node, key, value);
    }
    
    // This is an internal node, need to recurse into the appropriate child
    // But first, check if the child is full and split it if necessary
    btree_node_t *child = node->children[i];
    
    if (child && child->num_keys >= 2 * BTREE_ORDER - 1) {
        // Child is full, split it
        btree_split_child(node, i);
        
        // After splitting, the key we're looking for might be in the new child
        // Re-check which child to go into
        if (strcmp(key, node->keys[i]) > 0) {
            i++;
        }
    }
    
    // Recurse into the appropriate child
    if (node->children[i]) {
        return btree_insert_non_full(node->children[i], key, value);
    }
    
    return -1;
}

// Insert a key-value pair into the B-tree
// Handles root splitting and delegates to recursive insert function
// Returns 0 on success, -1 on error
int btree_insert(btree_t *tree, const char *key, const char *value) {
    if (!tree || !key || !value) {
        return -1;
    }
    
    // Check if root is full
    if (tree->root->num_keys >= 2 * BTREE_ORDER - 1) {
        // Root is full, need to split it
        // Create a new root node
        btree_node_t *new_root = btree_create_node(false);
        if (!new_root) {
            return -1;  // Memory allocation failed
        }
        
        // Make the old root a child of the new root
        new_root->children[0] = tree->root;
        tree->root->parent = new_root;
        
        // Split the old root (which is now child 0 of new root)
        btree_split_child(new_root, 0);
        
        // Update tree root and height
        tree->root = new_root;
        tree->height++;
        
        // After splitting, determine which child to insert into
        uint32_t i = 0;
        if (strcmp(key, new_root->keys[0]) > 0) {
            i = 1;
        }
        
        // Insert into the appropriate child
        return btree_insert_non_full(new_root->children[i], key, value);
    }
    
    // Root is not full, insert directly
    return btree_insert_non_full(tree->root, key, value);
}

int btree_delete(btree_t *tree, const char *key) {
    if (!tree || !key) {
        return -1;
    }
    
    // TODO: Implement B-tree delete
    // Handle various cases:
    // - Key in leaf node
    // - Key in internal node
    // - Merge nodes if needed
    
    return -1;
}

void btree_print(btree_t *tree) {
    if (!tree || !tree->root) {
        printf("Empty tree\n");
        return;
    }
    
    // TODO: Implement tree printing (for debugging)
    printf("B-tree (height: %u, keys in root: %u)\n", 
           tree->height, tree->root->num_keys);
}
