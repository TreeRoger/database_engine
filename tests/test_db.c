#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
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

// Test transaction commit
void test_transaction_commit(void) {
    printf("Test 6: Transaction commit\n");
    
    const char *path = "test_tx.db";
    remove(path);
    remove("test_tx.db.wal");
    
    db_t *db = NULL;
    assert(db_open(path, &db) == DB_SUCCESS);
    assert(db_begin(db) == DB_SUCCESS);
    assert(db_insert(db, "tx_key", "tx_value") == DB_SUCCESS);
    assert(db_commit(db) == DB_SUCCESS);
    char *v = NULL;
    assert(db_get(db, "tx_key", &v) == DB_SUCCESS);
    assert(strcmp(v, "tx_value") == 0);
    free(v);
    db_close(db);
    
    assert(db_open(path, &db) == DB_SUCCESS);
    assert(db_get(db, "tx_key", &v) == DB_SUCCESS);
    assert(strcmp(v, "tx_value") == 0);
    free(v);
    db_close(db);
    
    remove(path);
    remove("test_tx.db.wal");
    printf("  PASSED\n");
}

// Test transaction rollback
void test_transaction_rollback(void) {
    printf("Test 7: Transaction rollback\n");
    
    db_t *db = NULL;
    db_open("test.db", &db);
    assert(db != NULL);
    assert(db_insert(db, "keep", "value") == DB_SUCCESS);
    assert(db_commit(db) == DB_SUCCESS);
    
    assert(db_begin(db) == DB_SUCCESS);
    assert(db_insert(db, "rollback_key", "rollback_value") == DB_SUCCESS);
    assert(db_delete(db, "keep") == DB_SUCCESS);
    assert(db_rollback(db) == DB_SUCCESS);
    
    char *v = NULL;
    assert(db_get(db, "rollback_key", &v) == DB_NOT_FOUND);
    assert(db_get(db, "keep", &v) == DB_SUCCESS);
    assert(strcmp(v, "value") == 0);
    free(v);
    db_close(db);
    printf("  PASSED\n");
}

// Test persistence: data survives close and reopen
void test_persistence(void) {
    printf("Test 8: Persistence\n");
    
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
    printf("Test 9: Many insertions\n");
    
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

#define CONCURRENT_THREADS 8
#define CONCURRENT_INSERTS 20
static db_t *g_db_concurrent = NULL;

static void *thread_insert_and_get(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < CONCURRENT_INSERTS; i++) {
        char key[64];
        char value[64];
        snprintf(key, sizeof(key), "t%ld_k%d", id, i);
        snprintf(value, sizeof(value), "v%ld_%d", id, i);
        assert(db_insert(g_db_concurrent, key, value) == DB_SUCCESS);
        char *v = NULL;
        assert(db_get(g_db_concurrent, key, &v) == DB_SUCCESS);
        assert(strcmp(v, value) == 0);
        free(v);
    }
    return NULL;
}

// Test concurrent reader-writer access
void test_concurrent_access(void) {
    printf("Test 10: Concurrent access\n");
    
    const char *path = "test_concurrent.db";
    remove(path);
    remove("test_concurrent.db.wal");
    
    g_db_concurrent = NULL;
    assert(db_open(path, &g_db_concurrent) == DB_SUCCESS);
    
    pthread_t threads[CONCURRENT_THREADS];
    for (long t = 0; t < CONCURRENT_THREADS; t++) {
        assert(pthread_create(&threads[t], NULL, thread_insert_and_get, (void *)t) == 0);
    }
    for (long t = 0; t < CONCURRENT_THREADS; t++) {
        assert(pthread_join(threads[t], NULL) == 0);
    }
    
    for (long t = 0; t < CONCURRENT_THREADS; t++) {
        for (int i = 0; i < CONCURRENT_INSERTS; i++) {
            char key[64];
            char expected[64];
            snprintf(key, sizeof(key), "t%ld_k%d", t, i);
            snprintf(expected, sizeof(expected), "v%ld_%d", t, i);
            char *v = NULL;
            assert(db_get(g_db_concurrent, key, &v) == DB_SUCCESS);
            assert(strcmp(v, expected) == 0);
            free(v);
        }
    }
    
    db_close(g_db_concurrent);
    g_db_concurrent = NULL;
    remove(path);
    remove("test_concurrent.db.wal");
    printf("  PASSED\n");
}

static int g_range_count;
static char *g_range_keys[16];
static char *g_range_values[16];

static void range_cb(const char *key, const char *value, void *ctx) {
    (void)ctx;
    if (g_range_count < 16) {
        g_range_keys[g_range_count] = strdup(key);
        g_range_values[g_range_count] = strdup(value);
        g_range_count++;
    }
}

// Test range scan and full scan (query interface)
void test_range_and_select_all(void) {
    printf("Test 11: Range scan and SELECT *\n");
    
    const char *path = "test_range.db";
    remove(path);
    remove("test_range.db.wal");
    
    db_t *db = NULL;
    assert(db_open(path, &db) == DB_SUCCESS);
    assert(db_insert(db, "a", "1") == DB_SUCCESS);
    assert(db_insert(db, "b", "2") == DB_SUCCESS);
    assert(db_insert(db, "c", "3") == DB_SUCCESS);
    assert(db_insert(db, "d", "4") == DB_SUCCESS);
    assert(db_insert(db, "e", "5") == DB_SUCCESS);
    
    g_range_count = 0;
    db_range(db, "b", "d", range_cb, NULL);
    assert(g_range_count == 3);
    assert(strcmp(g_range_keys[0], "b") == 0 && strcmp(g_range_values[0], "2") == 0);
    assert(strcmp(g_range_keys[1], "c") == 0 && strcmp(g_range_values[1], "3") == 0);
    assert(strcmp(g_range_keys[2], "d") == 0 && strcmp(g_range_values[2], "4") == 0);
    for (int i = 0; i < g_range_count; i++) {
        free(g_range_keys[i]);
        free(g_range_values[i]);
    }
    
    g_range_count = 0;
    db_range(db, NULL, NULL, range_cb, NULL);
    assert(g_range_count == 5);
    for (int i = 0; i < g_range_count; i++) {
        free(g_range_keys[i]);
        free(g_range_values[i]);
    }
    
    db_close(db);
    remove(path);
    remove("test_range.db.wal");
    printf("  PASSED\n");
}

int main(void) {
    printf("Running database API tests...\n\n");
    
    remove("test.db");
    remove("test.db.wal");
    remove("test_many.db");
    remove("test_many.db.wal");
    remove("test_persist.db");
    remove("test_persist.db.wal");
    remove("test_tx.db");
    remove("test_tx.db.wal");
    remove("test_concurrent.db");
    remove("test_concurrent.db.wal");
    remove("test_range.db");
    remove("test_range.db.wal");
    
    test_db_open_close();
    test_insert_get();
    test_insert_duplicate();
    test_update();
    test_delete();
    test_transaction_commit();
    test_transaction_rollback();
    test_persistence();
    test_many_insertions();
    test_concurrent_access();
    test_range_and_select_all();
    
    printf("\nAll database tests passed!\n");
    return 0;
}
