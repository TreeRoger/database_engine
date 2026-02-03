#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/db.h"

// Test database open and close
void test_db_open_close(void) {
    printf("Test 1: Database open and close\n");
    
    db_t *db = NULL;
    db_result_t result = db_open("test.db", &db);
    assert(result == DB_SUCCESS);
    assert(db != NULL);
    
    result = db_close(db);
    assert(result == DB_SUCCESS);
    
    printf("  PASSED\n");
}

// Test insert and get operations
void test_insert_get(void) {
    printf("Test 2: Insert and get operations\n");
    
    db_t *db = NULL;
    db_open("test.db", &db);
    assert(db != NULL);
    
    // Insert some key-value pairs
    assert(db_insert(db, "name", "Alice") == DB_SUCCESS);
    assert(db_insert(db, "age", "30") == DB_SUCCESS);
    assert(db_insert(db, "city", "New York") == DB_SUCCESS);
    
    // Retrieve values
    char *value = NULL;
    assert(db_get(db, "name", &value) == DB_SUCCESS);
    assert(strcmp(value, "Alice") == 0);
    free(value);
    
    assert(db_get(db, "age", &value) == DB_SUCCESS);
    assert(strcmp(value, "30") == 0);
    free(value);
    
    assert(db_get(db, "city", &value) == DB_SUCCESS);
    assert(strcmp(value, "New York") == 0);
    free(value);
    
    // Try to get non-existent key
    assert(db_get(db, "nonexistent", &value) == DB_NOT_FOUND);
    
    db_close(db);
    printf("  PASSED\n");
}

// Test insert duplicate key
void test_insert_duplicate(void) {
    printf("Test 3: Insert duplicate key\n");
    
    db_t *db = NULL;
    db_open("test.db", &db);
    assert(db != NULL);
    
    // Insert a key
    assert(db_insert(db, "key1", "value1") == DB_SUCCESS);
    
    // Try to insert same key again
    assert(db_insert(db, "key1", "value2") == DB_EXISTS);
    
    // Original value should still be there
    char *value = NULL;
    assert(db_get(db, "key1", &value) == DB_SUCCESS);
    assert(strcmp(value, "value1") == 0);
    free(value);
    
    db_close(db);
    printf("  PASSED\n");
}

// Test update operation
void test_update(void) {
    printf("Test 4: Update operation\n");
    
    db_t *db = NULL;
    db_open("test.db", &db);
    assert(db != NULL);
    
    // Insert a key
    assert(db_insert(db, "color", "blue") == DB_SUCCESS);
    
    // Verify initial value
    char *value = NULL;
    assert(db_get(db, "color", &value) == DB_SUCCESS);
    assert(strcmp(value, "blue") == 0);
    free(value);
    
    // Update the value
    assert(db_update(db, "color", "green") == DB_SUCCESS);
    
    // Verify updated value
    assert(db_get(db, "color", &value) == DB_SUCCESS);
    assert(strcmp(value, "green") == 0);
    free(value);
    
    // Try to update non-existent key
    assert(db_update(db, "nonexistent", "value") == DB_NOT_FOUND);
    
    db_close(db);
    printf("  PASSED\n");
}

// Test delete operation
void test_delete(void) {
    printf("Test 5: Delete operation\n");
    
    db_t *db = NULL;
    db_open("test.db", &db);
    assert(db != NULL);
    
    // Insert keys
    assert(db_insert(db, "to_delete", "value") == DB_SUCCESS);
    char *value = NULL;
    assert(db_get(db, "to_delete", &value) == DB_SUCCESS);
    free(value);
    
    // Delete
    assert(db_delete(db, "to_delete") == DB_SUCCESS);
    assert(db_get(db, "to_delete", &value) == DB_NOT_FOUND);
    
    // Delete non-existent key
    assert(db_delete(db, "nonexistent") == DB_NOT_FOUND);
    
    db_close(db);
    printf("  PASSED\n");
}

// Test persistence: data survives close and reopen
void test_persistence(void) {
    printf("Test 6: Persistence\n");
    
    const char *path = "test_persist.db";
    remove(path);
    
    db_t *db = NULL;
    assert(db_open(path, &db) == DB_SUCCESS);
    assert(db_insert(db, "saved_key", "saved_value") == DB_SUCCESS);
    assert(db_insert(db, "another", "data") == DB_SUCCESS);
    db_close(db);
    
    assert(db_open(path, &db) == DB_SUCCESS);
    char *v = NULL;
    assert(db_get(db, "saved_key", &v) == DB_SUCCESS);
    assert(strcmp(v, "saved_value") == 0);
    free(v);
    assert(db_get(db, "another", &v) == DB_SUCCESS);
    assert(strcmp(v, "data") == 0);
    free(v);
    db_close(db);
    
    remove(path);
    printf("  PASSED\n");
}

// Test many insertions to verify B-tree works
void test_many_insertions(void) {
    printf("Test 7: Many insertions\n");
    
    db_t *db = NULL;
    db_open("test_many.db", &db);
    assert(db != NULL);
    
    // Insert 50 key-value pairs
    for (int i = 0; i < 50; i++) {
        char key[32];
        char value[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        assert(db_insert(db, key, value) == DB_SUCCESS);
    }
    
    // Verify all can be retrieved
    for (int i = 0; i < 50; i++) {
        char key[32];
        char expected_value[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(expected_value, sizeof(expected_value), "value%d", i);
        
        char *value = NULL;
        assert(db_get(db, key, &value) == DB_SUCCESS);
        assert(strcmp(value, expected_value) == 0);
        free(value);
    }
    
    db_close(db);
    printf("  PASSED\n");
}

int main(void) {
    printf("Running database API tests...\n\n");
    
    remove("test.db");
    remove("test_many.db");
    remove("test_persist.db");
    
    test_db_open_close();
    test_insert_get();
    test_insert_duplicate();
    test_update();
    test_delete();
    test_persistence();
    test_many_insertions();
    
    printf("\nAll database tests passed!\n");
    return 0;
}
