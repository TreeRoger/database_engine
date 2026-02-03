#ifndef DB_H
#define DB_H

#include <stdint.h>
#include <stdbool.h>

// Forward declaration
struct btree_t;
struct page_manager_t;

// Database handle - opaque structure
typedef struct db_impl db_t;

// Result codes
typedef enum {
    DB_SUCCESS = 0,
    DB_ERROR = -1,
    DB_NOT_FOUND = -2,
    DB_EXISTS = -3,
    DB_FULL = -4,
} db_result_t;

// Initialize a new database
db_result_t db_open(const char *filename, db_t **db);

// Close the database
db_result_t db_close(db_t *db);

// Insert a key-value pair
db_result_t db_insert(db_t *db, const char *key, const char *value);

// Get a value by key
db_result_t db_get(db_t *db, const char *key, char **value);

// Delete a key-value pair
db_result_t db_delete(db_t *db, const char *key);

// Update a key-value pair
db_result_t db_update(db_t *db, const char *key, const char *value);

// Transaction support (Phase 2)
db_result_t db_begin(db_t *db);
db_result_t db_commit(db_t *db);
db_result_t db_rollback(db_t *db);

// Query / range scan (Phase 4)
// Callback for each key-value pair; NULL start or end means no bound
void db_range(db_t *db, const char *start_key, const char *end_key,
    void (*cb)(const char *key, const char *value, void *ctx), void *ctx);

#endif // DB_H
