#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "db.h"
#include "btree.h"
#include "page.h"

// Database structure implementation
struct db_impl {
    char *filename;
    page_manager_t *pm;      // Page manager for persistence (optional for now)
    btree_t *index;           // B-tree index for key-value storage
    bool persistent;          // Whether persistence is enabled
};

db_result_t db_open(const char *filename, db_t **db) {
    if (!filename || !db) {
        return DB_ERROR;
    }
    
    // Allocate database structure
    db_t *new_db = calloc(1, sizeof(struct db_impl));
    if (!new_db) {
        return DB_ERROR;
    }
    
    // Store filename
    new_db->filename = strdup(filename);
    if (!new_db->filename) {
        free(new_db);
        return DB_ERROR;
    }
    
    // Create B-tree index (in-memory for now)
    new_db->index = btree_create();
    if (!new_db->index) {
        free(new_db->filename);
        free(new_db);
        return DB_ERROR;
    }
    
    // Page manager initialization is optional for now
    // We'll add persistence later
    new_db->pm = NULL;
    new_db->persistent = false;
    
    *db = new_db;
    return DB_SUCCESS;
}

db_result_t db_close(db_t *db) {
    if (!db) {
        return DB_ERROR;
    }
    
    // Free B-tree and all its data
    if (db->index) {
        btree_destroy(db->index);
        db->index = NULL;
    }
    
    // Close page manager if it was initialized
    if (db->pm) {
        page_manager_destroy(db->pm);
        db->pm = NULL;
    }
    
    // Free filename
    if (db->filename) {
        free(db->filename);
        db->filename = NULL;
    }
    
    // Free database structure
    free(db);
    
    return DB_SUCCESS;
}

db_result_t db_insert(db_t *db, const char *key, const char *value) {
    if (!db || !key || !value) {
        return DB_ERROR;
    }
    
    // Check if key already exists
    char *existing_value = btree_search(db->index, key);
    if (existing_value != NULL) {
        free(existing_value);
        return DB_EXISTS;
    }
    
    // Insert into B-tree
    int result = btree_insert(db->index, key, value);
    if (result != 0) {
        return DB_ERROR;
    }
    
    // TODO: Write to disk via page manager when persistence is implemented
    
    return DB_SUCCESS;
}

db_result_t db_get(db_t *db, const char *key, char **value) {
    if (!db || !key || !value) {
        return DB_ERROR;
    }
    
    // Search B-tree for key
    char *found_value = btree_search(db->index, key);
    if (found_value == NULL) {
        return DB_NOT_FOUND;
    }
    
    // Return the value (caller is responsible for freeing it)
    *value = found_value;
    return DB_SUCCESS;
}

db_result_t db_delete(db_t *db, const char *key) {
    if (!db || !key) {
        return DB_ERROR;
    }
    
    // Check if key exists first
    char *existing_value = btree_search(db->index, key);
    if (existing_value == NULL) {
        return DB_NOT_FOUND;
    }
    free(existing_value);
    
    // Delete from B-tree
    // Note: btree_delete is not yet implemented, so this will fail for now
    // Once delete is implemented, this will work
    int result = btree_delete(db->index, key);
    if (result != 0) {
        return DB_ERROR;
    }
    
    // TODO: Free associated pages when persistence is implemented
    
    return DB_SUCCESS;
}

db_result_t db_update(db_t *db, const char *key, const char *value) {
    if (!db || !key || !value) {
        return DB_ERROR;
    }
    
    // Check if key exists
    char *existing_value = btree_search(db->index, key);
    if (existing_value == NULL) {
        return DB_NOT_FOUND;
    }
    free(existing_value);
    
    // Update value in B-tree (insert will update if key exists)
    int result = btree_insert(db->index, key, value);
    if (result != 0) {
        return DB_ERROR;
    }
    
    // TODO: Write to disk when persistence is implemented
    
    return DB_SUCCESS;
}
