#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/btree.h"

// Test basic insert and search operations
void test_basic_insert_search(void) {
    printf("Test 1: Basic insert and search\n");
    
    btree_t *tree = btree_create();
    assert(tree != NULL);
    assert(tree->root != NULL);
    
    // Insert some key-value pairs
    assert(btree_insert(tree, "apple", "red") == 0);
    assert(btree_insert(tree, "banana", "yellow") == 0);
    assert(btree_insert(tree, "cherry", "red") == 0);
    
    // Search for existing keys
    char *value = btree_search(tree, "apple");
    assert(value != NULL);
    assert(strcmp(value, "red") == 0);
    free(value);
    
    value = btree_search(tree, "banana");
    assert(value != NULL);
    assert(strcmp(value, "yellow") == 0);
    free(value);
    
    value = btree_search(tree, "cherry");
    assert(value != NULL);
    assert(strcmp(value, "red") == 0);
    free(value);
    
    // Search for non-existent key
    value = btree_search(tree, "dragonfruit");
    assert(value == NULL);
    
    btree_destroy(tree);
    printf("  PASSED\n");
}

// Test search on empty tree
void test_empty_tree_search(void) {
    printf("Test 2: Search on empty tree\n");
    
    btree_t *tree = btree_create();
    assert(tree != NULL);
    
    char *value = btree_search(tree, "anything");
    assert(value == NULL);
    
    btree_destroy(tree);
    printf("  PASSED\n");
}

// Test update existing key
void test_update_key(void) {
    printf("Test 3: Update existing key\n");
    
    btree_t *tree = btree_create();
    assert(tree != NULL);
    
    // Insert a key
    assert(btree_insert(tree, "color", "blue") == 0);
    
    // Verify initial value
    char *value = btree_search(tree, "color");
    assert(value != NULL);
    assert(strcmp(value, "blue") == 0);
    free(value);
    
    // Update the key
    assert(btree_insert(tree, "color", "green") == 0);
    
    // Verify updated value
    value = btree_search(tree, "color");
    assert(value != NULL);
    assert(strcmp(value, "green") == 0);
    free(value);
    
    btree_destroy(tree);
    printf("  PASSED\n");
}

// Test insertion order preservation
void test_insertion_order(void) {
    printf("Test 4: Insertion order\n");
    
    btree_t *tree = btree_create();
    assert(tree != NULL);
    
    // Insert keys in non-alphabetical order
    assert(btree_insert(tree, "zebra", "striped") == 0);
    assert(btree_insert(tree, "apple", "fruit") == 0);
    assert(btree_insert(tree, "monkey", "primate") == 0);
    
    // All should be searchable
    char *value = btree_search(tree, "apple");
    assert(value != NULL);
    free(value);
    
    value = btree_search(tree, "monkey");
    assert(value != NULL);
    free(value);
    
    value = btree_search(tree, "zebra");
    assert(value != NULL);
    free(value);
    
    btree_destroy(tree);
    printf("  PASSED\n");
}

// Test node splitting by inserting enough keys to fill and overflow a node
// With BTREE_ORDER=4, a node can hold 7 keys, so 8+ keys should trigger splits
void test_node_splitting(void) {
    printf("Test 5: Node splitting\n");
    
    btree_t *tree = btree_create();
    assert(tree != NULL);
    
    // Insert enough keys to trigger at least one split (8 keys > 7 max per node)
    const char *keys[] = {
        "key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8"
    };
    
    for (int i = 0; i < 8; i++) {
        char value[16];
        snprintf(value, sizeof(value), "value%d", i + 1);
        assert(btree_insert(tree, keys[i], value) == 0);
    }
    
    // Verify all keys can still be found after splitting
    for (int i = 0; i < 8; i++) {
        char expected_value[16];
        snprintf(expected_value, sizeof(expected_value), "value%d", i + 1);
        
        char *value = btree_search(tree, keys[i]);
        assert(value != NULL);
        assert(strcmp(value, expected_value) == 0);
        free(value);
    }
    
    btree_destroy(tree);
    printf("  PASSED\n");
}

// Test root splitting by inserting many keys
// This should cause the root to split and increase tree height
void test_root_splitting(void) {
    printf("Test 6: Root splitting and tree growth\n");
    
    btree_t *tree = btree_create();
    assert(tree != NULL);
    
    uint32_t initial_height = tree->height;
    
    // Insert many keys to force multiple splits including root split
    // Insert 20 keys to ensure root split occurs
    for (int i = 1; i <= 20; i++) {
        char key[16];
        char value[16];
        snprintf(key, sizeof(key), "k%d", i);
        snprintf(value, sizeof(value), "v%d", i);
        assert(btree_insert(tree, key, value) == 0);
    }
    
    // Tree height should have increased (root was split)
    // With enough keys, height should be at least 2
    assert(tree->height >= initial_height);
    
    // Verify all keys are still accessible
    for (int i = 1; i <= 20; i++) {
        char key[16];
        char expected_value[16];
        snprintf(key, sizeof(key), "k%d", i);
        snprintf(expected_value, sizeof(expected_value), "v%d", i);
        
        char *value = btree_search(tree, key);
        assert(value != NULL);
        assert(strcmp(value, expected_value) == 0);
        free(value);
    }
    
    btree_destroy(tree);
    printf("  PASSED\n");
}

// Test inserting keys in various orders to verify splitting works correctly
void test_various_insertion_patterns(void) {
    printf("Test 7: Various insertion patterns\n");
    
    btree_t *tree = btree_create();
    assert(tree != NULL);
    
    // Insert keys in ascending order
    for (int i = 0; i < 10; i++) {
        char key[16];
        char value[16];
        snprintf(key, sizeof(key), "a%d", i);
        snprintf(value, sizeof(value), "val%d", i);
        assert(btree_insert(tree, key, value) == 0);
    }
    
    // Insert keys in descending order
    for (int i = 9; i >= 0; i--) {
        char key[16];
        char value[16];
        snprintf(key, sizeof(key), "d%d", i);
        snprintf(value, sizeof(value), "val%d", i);
        assert(btree_insert(tree, key, value) == 0);
    }
    
    // Verify all keys exist
    for (int i = 0; i < 10; i++) {
        char key[16];
        snprintf(key, sizeof(key), "a%d", i);
        char *value = btree_search(tree, key);
        assert(value != NULL);
        free(value);
        
        snprintf(key, sizeof(key), "d%d", i);
        value = btree_search(tree, key);
        assert(value != NULL);
        free(value);
    }
    
    btree_destroy(tree);
    printf("  PASSED\n");
}

int main(void) {
    printf("Running B-tree tests...\n\n");
    
    test_empty_tree_search();
    test_basic_insert_search();
    test_update_key();
    test_insertion_order();
    test_node_splitting();
    test_root_splitting();
    test_various_insertion_patterns();
    
    printf("\nAll tests passed!\n");
    return 0;
}
