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

// Borrow a key from the left sibling (sibling has at least BTREE_ORDER keys)
// Child is at index child_index in parent
static void btree_borrow_from_left(btree_node_t *parent, uint32_t child_index) {
    btree_node_t *child = parent->children[child_index];
    btree_node_t *left_sibling = parent->children[child_index - 1];
    
    // Shift child's keys and values right to make room at the beginning
    for (uint32_t j = child->num_keys; j > 0; j--) {
        child->keys[j] = child->keys[j - 1];
        child->values[j] = child->values[j - 1];
    }
    if (!child->is_leaf) {
        for (uint32_t j = child->num_keys + 1; j > 0; j--) {
            child->children[j] = child->children[j - 1];
        }
    }
    
    // Move parent key down to child
    child->keys[0] = parent->keys[child_index - 1];
    child->values[0] = parent->values[child_index - 1];
    
    // Move rightmost key from left sibling up to parent
    parent->keys[child_index - 1] = left_sibling->keys[left_sibling->num_keys - 1];
    parent->values[child_index - 1] = left_sibling->values[left_sibling->num_keys - 1];
    
    if (!child->is_leaf) {
        child->children[0] = left_sibling->children[left_sibling->num_keys];
        if (child->children[0]) {
            child->children[0]->parent = child;
        }
        left_sibling->children[left_sibling->num_keys] = NULL;
    }
    
    left_sibling->keys[left_sibling->num_keys - 1] = NULL;
    left_sibling->values[left_sibling->num_keys - 1] = NULL;
    left_sibling->num_keys--;
    child->num_keys++;
}

// Borrow a key from the right sibling (sibling has at least BTREE_ORDER keys)
static void btree_borrow_from_right(btree_node_t *parent, uint32_t child_index) {
    btree_node_t *child = parent->children[child_index];
    btree_node_t *right_sibling = parent->children[child_index + 1];
    
    // Move parent key down to child
    child->keys[child->num_keys] = parent->keys[child_index];
    child->values[child->num_keys] = parent->values[child_index];
    
    // Move leftmost key from right sibling up to parent
    parent->keys[child_index] = right_sibling->keys[0];
    parent->values[child_index] = right_sibling->values[0];
    
    if (!child->is_leaf) {
        child->children[child->num_keys + 1] = right_sibling->children[0];
        if (child->children[child->num_keys + 1]) {
            child->children[child->num_keys + 1]->parent = child;
        }
    }
    
    // Shift right sibling's keys and values left
    for (uint32_t j = 0; j < right_sibling->num_keys - 1; j++) {
        right_sibling->keys[j] = right_sibling->keys[j + 1];
        right_sibling->values[j] = right_sibling->values[j + 1];
    }
    right_sibling->keys[right_sibling->num_keys - 1] = NULL;
    right_sibling->values[right_sibling->num_keys - 1] = NULL;
    
    if (!right_sibling->is_leaf) {
        for (uint32_t j = 0; j < right_sibling->num_keys; j++) {
            right_sibling->children[j] = right_sibling->children[j + 1];
        }
        right_sibling->children[right_sibling->num_keys] = NULL;
    }
    
    right_sibling->num_keys--;
    child->num_keys++;
}

// Merge child at index child_index with its right sibling
// After merge, the right sibling is freed and child contains all keys
static void btree_merge_children(btree_node_t *parent, uint32_t child_index) {
    btree_node_t *child = parent->children[child_index];
    btree_node_t *right_sibling = parent->children[child_index + 1];
    
    // Bring down the key from parent between the two children
    child->keys[child->num_keys] = parent->keys[child_index];
    child->values[child->num_keys] = parent->values[child_index];
    child->num_keys++;
    
    // Copy all keys and values from right sibling to child
    for (uint32_t j = 0; j < right_sibling->num_keys; j++) {
        child->keys[child->num_keys] = right_sibling->keys[j];
        child->values[child->num_keys] = right_sibling->values[j];
        right_sibling->keys[j] = NULL;
        right_sibling->values[j] = NULL;
        child->num_keys++;
    }
    
    if (!child->is_leaf) {
        // Child has num_keys keys now; right_sibling's children go at indices
        // (child->num_keys - right_sibling->num_keys) to (child->num_keys - right_sibling->num_keys + right_sibling->num_keys)
        uint32_t base = child->num_keys - right_sibling->num_keys;
        for (uint32_t j = 0; j <= right_sibling->num_keys; j++) {
            child->children[base + j] = right_sibling->children[j];
            if (right_sibling->children[j]) {
                right_sibling->children[j]->parent = child;
            }
            right_sibling->children[j] = NULL;
        }
    }
    
    // Remove the key and child from parent (shift left)
    for (uint32_t j = child_index; j < parent->num_keys - 1; j++) {
        parent->keys[j] = parent->keys[j + 1];
        parent->values[j] = parent->values[j + 1];
        parent->children[j + 1] = parent->children[j + 2];
    }
    parent->keys[parent->num_keys - 1] = NULL;
    parent->values[parent->num_keys - 1] = NULL;
    parent->children[parent->num_keys] = NULL;
    parent->num_keys--;
    
    // Free the right sibling (but not its contents - they were moved)
    free(right_sibling->keys);
    free(right_sibling->values);
    free(right_sibling->children);
    free(right_sibling);
}

// Remove key at index i from a leaf node (caller ensures node has enough keys)
static void btree_remove_from_leaf(btree_node_t *node, uint32_t i) {
    free(node->keys[i]);
    free(node->values[i]);
    node->keys[i] = NULL;
    node->values[i] = NULL;
    
    for (uint32_t j = i; j < node->num_keys - 1; j++) {
        node->keys[j] = node->keys[j + 1];
        node->values[j] = node->values[j + 1];
    }
    node->keys[node->num_keys - 1] = NULL;
    node->values[node->num_keys - 1] = NULL;
    node->num_keys--;
}

// Get predecessor: rightmost key in the left subtree of key at index i
static btree_node_t* btree_get_predecessor_leaf(btree_node_t *node, uint32_t i) {
    btree_node_t *curr = node->children[i];
    while (curr && !curr->is_leaf) {
        curr = curr->children[curr->num_keys];
    }
    return curr;
}

// Recursive delete: delete key from subtree rooted at node
// Returns 0 if key was found and deleted, -1 if key not found
static int btree_delete_from_node(btree_t *tree, btree_node_t *node, const char *key) {
    if (!node || !key) {
        return -1;
    }
    
    uint32_t i = btree_search_node_index(node, key);
    
    // Case 1: Key is present in this node
    if (i < node->num_keys && strcmp(key, node->keys[i]) == 0) {
        if (node->is_leaf) {
            // Case 1a: Key is in a leaf - remove it directly
            btree_remove_from_leaf(node, i);
            return 0;
        } else {
            // Case 1b: Key is in an internal node
            // Replace with predecessor (rightmost in left subtree), then recursively
            // delete the predecessor from the leaf (recursion ensures sufficient keys)
            btree_node_t *pred_leaf = btree_get_predecessor_leaf(node, i);
            if (pred_leaf && pred_leaf->num_keys > 0) {
                uint32_t pred_idx = pred_leaf->num_keys - 1;
                char *pred_key = pred_leaf->keys[pred_idx];
                if (pred_key) {
                    free(node->keys[i]);
                    free(node->values[i]);
                    node->keys[i] = strdup(pred_key);
                    node->values[i] = pred_leaf->values[pred_idx];
                    pred_leaf->values[pred_idx] = NULL;
                    // Key stays in leaf so recursive delete can find and remove it
                    return btree_delete_from_node(tree, node->children[i], pred_key);
                }
            }
            return -1;
        }
    }
    
    // Case 2: Key is not in this node
    if (node->is_leaf) {
        return -1;  // Key not found
    }
    
    // Ensure child at i has at least BTREE_ORDER keys before descending
    btree_node_t *child = node->children[i];
    if (!child) {
        return -1;
    }
    
    if (child->num_keys < BTREE_ORDER) {
        uint32_t child_idx = i;
        btree_node_t *left_sib = (child_idx > 0) ? node->children[child_idx - 1] : NULL;
        btree_node_t *right_sib = (child_idx < node->num_keys) ? node->children[child_idx + 1] : NULL;
        
        if (left_sib && left_sib->num_keys >= BTREE_ORDER) {
            btree_borrow_from_left(node, child_idx);
        } else if (right_sib && right_sib->num_keys >= BTREE_ORDER) {
            btree_borrow_from_right(node, child_idx);
        } else if (left_sib) {
            btree_merge_children(node, child_idx - 1);
            child = node->children[child_idx - 1];
            if (node == tree->root && node->num_keys == 0) {
                tree->root = child;
                child->parent = NULL;
                free(node->keys);
                free(node->values);
                free(node->children);
                free(node);
                tree->height--;
            }
            return btree_delete_from_node(tree, child, key);
        } else if (right_sib) {
            btree_merge_children(node, child_idx);
            if (node == tree->root && node->num_keys == 0) {
                tree->root = child;
                child->parent = NULL;
                free(node->keys);
                free(node->values);
                free(node->children);
                free(node);
                tree->height--;
            }
            return btree_delete_from_node(tree, child, key);
        }
    }
    
    return btree_delete_from_node(tree, child, key);
}

// Delete a key from the B-tree
// Returns 0 on success, -1 if key not found or on error
int btree_delete(btree_t *tree, const char *key) {
    if (!tree || !key || !tree->root) {
        return -1;
    }
    
    int result = btree_delete_from_node(tree, tree->root, key);
    
    // If root has 0 keys (and is not a leaf), replace root with its only child
    if (result == 0 && tree->root && !tree->root->is_leaf && tree->root->num_keys == 0) {
        btree_node_t *old_root = tree->root;
        tree->root = tree->root->children[0];
        if (tree->root) {
            tree->root->parent = NULL;
        }
        free(old_root->keys);
        free(old_root->values);
        free(old_root->children);
        free(old_root);
        tree->height--;
    }
    
    // If root is a leaf and has 0 keys, tree is empty - leave root as empty node
    // (btree_create gives us a root with 0 keys, so we allow that)
    
    return result;
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

// In-order traversal: visit left subtree, then key-value, then right subtree
static void btree_foreach_node(btree_node_t *node,
    void (*cb)(const char *key, const char *value, void *ctx), void *ctx) {
    if (!node || !cb) {
        return;
    }
    uint32_t i;
    for (i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf && node->children && node->children[i]) {
            btree_foreach_node(node->children[i], cb, ctx);
        }
        if (node->keys[i] && node->values[i]) {
            cb(node->keys[i], node->values[i], ctx);
        }
    }
    if (!node->is_leaf && node->children && node->children[i]) {
        btree_foreach_node(node->children[i], cb, ctx);
    }
}

void btree_foreach(btree_t *tree, void (*cb)(const char *key, const char *value, void *ctx), void *ctx) {
    if (!tree || !tree->root || !cb) {
        return;
    }
    btree_foreach_node(tree->root, cb, ctx);
}
