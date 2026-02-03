#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include "db.h"
#include "btree.h"
#include "page.h"

// Transaction undo log entry (LIFO: rollback undoes from first pushed)
typedef enum { TX_OP_INSERT, TX_OP_DELETE, TX_OP_UPDATE } tx_op_t;

typedef struct tx_entry {
    tx_op_t op;
    char *key;
    char *old_value;   /* for DELETE/UPDATE: value to restore on rollback */
    struct tx_entry *next;
} tx_entry_t;

// Database structure implementation
struct db_impl {
    char *filename;
    page_manager_t *pm;
    btree_t *index;
    bool persistent;
    bool in_transaction;
    tx_entry_t *undo_log;
    pthread_rwlock_t rwlock;   /* reader-writer lock for multi-threaded access */
};

// Metadata in page 0: first 4 bytes of data = number of data pages (1-based)
#define METADATA_NUM_PAGES_OFFSET 0
#define PAGE_DATA_CAPACITY (PAGE_SIZE - 16 - 4)  // data[] size minus 4-byte bytes_used
#define MAX_KEY_LEN 4096
#define MAX_VALUE_LEN 65536
#define WAL_TYPE_PUT  0
#define WAL_TYPE_DEL  1

static void tx_clear(db_t *db) {
    while (db->undo_log) {
        tx_entry_t *e = db->undo_log;
        db->undo_log = e->next;
        free(e->key);
        free(e->old_value);
        free(e);
    }
}

static void tx_rollback_apply(db_t *db) {
    tx_entry_t *e = db->undo_log;
    while (e) {
        if (e->op == TX_OP_INSERT) {
            btree_delete(db->index, e->key);
        } else if (e->op == TX_OP_DELETE && e->old_value) {
            btree_insert(db->index, e->key, e->old_value);
        } else if (e->op == TX_OP_UPDATE && e->old_value) {
            btree_insert(db->index, e->key, e->old_value);
        }
        e = e->next;
    }
}

static tx_entry_t* tx_push(db_t *db, tx_op_t op, const char *key, char *old_value) {
    tx_entry_t *e = malloc(sizeof(tx_entry_t));
    if (!e) return NULL;
    e->op = op;
    e->key = strdup(key);
    e->old_value = old_value;  /* caller gives ownership or NULL */
    e->next = db->undo_log;
    db->undo_log = e;
    return e;
}

// Replay WAL file into B-tree (after loading main DB)
static db_result_t db_replay_wal(db_t *db) {
    size_t fn_len = strlen(db->filename) + 8;
    char *wal_path = malloc(fn_len);
    if (!wal_path) return DB_ERROR;
    snprintf(wal_path, fn_len, "%s.wal", db->filename);
    int fd = open(wal_path, O_RDONLY);
    free(wal_path);
    if (fd < 0) return DB_SUCCESS;  /* no WAL file */
    db_result_t result = DB_SUCCESS;
    uint8_t type;
    uint32_t key_len, value_len;
    char *key = NULL;
    char *value = NULL;
    while (read(fd, &type, 1) == 1) {
        if (read(fd, &key_len, 4) != 4) { result = DB_ERROR; break; }
        if (key_len > MAX_KEY_LEN) { result = DB_ERROR; break; }
        free(key);
        key = malloc(key_len + 1);
        if (!key) { result = DB_ERROR; break; }
        if ((size_t)read(fd, key, key_len) != key_len) { result = DB_ERROR; break; }
        key[key_len] = '\0';
        if (type == WAL_TYPE_PUT) {
            if (read(fd, &value_len, 4) != 4) { result = DB_ERROR; break; }
            if (value_len > MAX_VALUE_LEN) { result = DB_ERROR; break; }
            free(value);
            value = malloc(value_len + 1);
            if (!value) { result = DB_ERROR; break; }
            if ((size_t)read(fd, value, value_len) != value_len) { result = DB_ERROR; break; }
            value[value_len] = '\0';
            btree_insert(db->index, key, value);
        } else if (type == WAL_TYPE_DEL) {
            btree_delete(db->index, key);
        }
    }
    free(key);
    free(value);
    close(fd);
    return result;
}

// Append current transaction to WAL and fsync
static db_result_t db_wal_commit(db_t *db) {
    size_t fn_len = strlen(db->filename) + 8;
    char *wal_path = malloc(fn_len);
    if (!wal_path) return DB_ERROR;
    snprintf(wal_path, fn_len, "%s.wal", db->filename);
    int fd = open(wal_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    free(wal_path);
    if (fd < 0) return DB_ERROR;
    /* Undo log is newest-first; write to WAL in chronological order (oldest first). */
    size_t n = 0;
    for (tx_entry_t *t = db->undo_log; t; t = t->next) n++;
    tx_entry_t **arr = malloc(n * sizeof(tx_entry_t *));
    if (!arr) { close(fd); return DB_ERROR; }
    size_t i = 0;
    for (tx_entry_t *t = db->undo_log; t; t = t->next) arr[i++] = t;
    for (i = n; i > 0; i--) {
        tx_entry_t *e = arr[i - 1];
        uint8_t type;
        uint32_t key_len = (uint32_t)strlen(e->key);
        if (e->op == TX_OP_INSERT) {
            type = WAL_TYPE_PUT;
            if (write(fd, &type, 1) != 1) { free(arr); close(fd); return DB_ERROR; }
            if (write(fd, &key_len, 4) != 4) { free(arr); close(fd); return DB_ERROR; }
            if (write(fd, e->key, key_len) != (ssize_t)key_len) { free(arr); close(fd); return DB_ERROR; }
            char *val = btree_search(db->index, e->key);
            if (val) {
                uint32_t value_len = (uint32_t)strlen(val);
                if (write(fd, &value_len, 4) != 4) { free(val); free(arr); close(fd); return DB_ERROR; }
                if (write(fd, val, value_len) != (ssize_t)value_len) { free(val); free(arr); close(fd); return DB_ERROR; }
                free(val);
            } else {
                uint32_t z = 0;
                if (write(fd, &z, 4) != 4) { free(arr); close(fd); return DB_ERROR; }
            }
        } else if (e->op == TX_OP_DELETE) {
            type = WAL_TYPE_DEL;
            if (write(fd, &type, 1) != 1) { free(arr); close(fd); return DB_ERROR; }
            if (write(fd, &key_len, 4) != 4) { free(arr); close(fd); return DB_ERROR; }
            if (write(fd, e->key, key_len) != (ssize_t)key_len) { free(arr); close(fd); return DB_ERROR; }
        } else if (e->op == TX_OP_UPDATE && e->old_value) {
            type = WAL_TYPE_PUT;
            if (write(fd, &type, 1) != 1) { free(arr); close(fd); return DB_ERROR; }
            if (write(fd, &key_len, 4) != 4) { free(arr); close(fd); return DB_ERROR; }
            if (write(fd, e->key, key_len) != (ssize_t)key_len) { free(arr); close(fd); return DB_ERROR; }
            uint32_t value_len = (uint32_t)strlen(e->old_value);
            if (write(fd, &value_len, 4) != 4) { free(arr); close(fd); return DB_ERROR; }
            if (write(fd, e->old_value, value_len) != (ssize_t)value_len) { free(arr); close(fd); return DB_ERROR; }
        }
    }
    free(arr);
    if (fsync(fd) != 0) { close(fd); return DB_ERROR; }
    close(fd);
    return DB_SUCCESS;
}

// Truncate WAL after checkpoint (save)
static void db_wal_checkpoint(db_t *db) {
    size_t fn_len = strlen(db->filename) + 8;
    char *wal_path = malloc(fn_len);
    if (!wal_path) return;
    snprintf(wal_path, fn_len, "%s.wal", db->filename);
    int fd = open(wal_path, O_WRONLY | O_TRUNC, 0644);
    free(wal_path);
    if (fd >= 0) close(fd);
}

// Load all key-value pairs from data pages into the B-tree
static db_result_t db_load_from_pages(db_t *db) {
    page_t meta;
    if (page_read(db->pm, 0, &meta) != 0) {
        return DB_ERROR;
    }
    uint32_t num_data_pages = 0;
    memcpy(&num_data_pages, meta.data + METADATA_NUM_PAGES_OFFSET, sizeof(uint32_t));
    if (num_data_pages == 0) {
        return DB_SUCCESS;
    }
    for (uint32_t p = 1; p <= num_data_pages; p++) {
        page_t page;
        if (page_read(db->pm, p, &page) != 0) {
            return DB_ERROR;
        }
        uint32_t bytes_used = 0;
        memcpy(&bytes_used, page.data, sizeof(uint32_t));
        uint32_t pos = 4;
        while (pos + 8 <= 4 + bytes_used) {
            uint32_t key_len, value_len;
            memcpy(&key_len, page.data + pos, 4);
            memcpy(&value_len, page.data + pos + 4, 4);
            pos += 8;
            if (key_len > MAX_KEY_LEN || value_len > MAX_VALUE_LEN || pos + key_len + value_len > 4 + bytes_used) {
                return DB_ERROR;
            }
            char *key_str = malloc(key_len + 1);
            char *value_str = malloc(value_len + 1);
            if (!key_str || !value_str) {
                free(key_str);
                free(value_str);
                return DB_ERROR;
            }
            memcpy(key_str, page.data + pos, key_len);
            key_str[key_len] = '\0';
            pos += key_len;
            memcpy(value_str, page.data + pos, value_len);
            value_str[value_len] = '\0';
            pos += value_len;
            btree_insert(db->index, key_str, value_str);
            free(key_str);
            free(value_str);
        }
    }
    return DB_SUCCESS;
}

// Pack (key, value) into buffer; returns bytes written or 0 if no room
static uint32_t pack_record(uint8_t *buf, uint32_t buf_len, uint32_t offset,
    const char *key, const char *value) {
    uint32_t key_len = (uint32_t)strlen(key);
    uint32_t value_len = (uint32_t)strlen(value);
    if (key_len > MAX_KEY_LEN || value_len > MAX_VALUE_LEN) {
        return 0;
    }
    uint32_t need = 8 + key_len + value_len;
    if (offset + need > buf_len) {
        return 0;
    }
    memcpy(buf + offset, &key_len, 4);
    memcpy(buf + offset + 4, &value_len, 4);
    memcpy(buf + offset + 8, key, key_len);
    memcpy(buf + offset + 8 + key_len, value, value_len);
    return need;
}

struct save_ctx {
    page_t *cur_page;
    uint32_t offset;
    uint32_t num_data_pages;
    uint32_t next_page_id;
    page_manager_t *pm;
};

static void save_cb(const char *key, const char *value, void *vctx) {
    struct save_ctx *c = (struct save_ctx *)vctx;
    uint32_t need = pack_record(c->cur_page->data, PAGE_DATA_CAPACITY + 4, c->offset, key, value);
    if (need == 0) {
        uint32_t bytes_used = c->offset - 4;
        memcpy(c->cur_page->data, &bytes_used, 4);
        if (c->next_page_id >= c->pm->num_pages) {
            page_alloc(c->pm);
        }
        c->cur_page->page_id = c->next_page_id;
        if (page_write(c->pm, c->next_page_id, c->cur_page) != 0) {
            return;
        }
        c->num_data_pages++;
        c->next_page_id++;
        memset(c->cur_page->data, 0, sizeof(c->cur_page->data));
        c->offset = 4;
        need = pack_record(c->cur_page->data, PAGE_DATA_CAPACITY + 4, c->offset, key, value);
        if (need == 0) {
            return;
        }
    }
    c->offset += need;
}

// Save B-tree to data pages and update metadata
static db_result_t db_save_to_pages(db_t *db) {
    uint8_t *page_buf = malloc(PAGE_SIZE);
    if (!page_buf) {
        return DB_ERROR;
    }
    page_t *cur_page = (page_t *)page_buf;
    memset(cur_page, 0, PAGE_SIZE);
    cur_page->type = PAGE_TYPE_DATA;

    struct save_ctx ctx = {
        .cur_page = cur_page,
        .offset = 4,
        .num_data_pages = 0,
        .next_page_id = 1,
        .pm = db->pm,
    };

    btree_foreach(db->index, save_cb, &ctx);

    uint32_t offset = ctx.offset;
    uint32_t num_data_pages = ctx.num_data_pages;

    if (offset > 4) {
        uint32_t bytes_used = offset - 4;
        memcpy(cur_page->data, &bytes_used, 4);
        if (ctx.next_page_id >= db->pm->num_pages) {
            page_alloc(db->pm);
        }
        cur_page->page_id = ctx.next_page_id;
        if (page_write(db->pm, ctx.next_page_id, cur_page) != 0) {
            free(page_buf);
            return DB_ERROR;
        }
        num_data_pages++;
    }

    page_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.type = PAGE_TYPE_METADATA;
    meta.page_id = 0;
    memcpy(meta.data + METADATA_NUM_PAGES_OFFSET, &num_data_pages, sizeof(uint32_t));
    page_write(db->pm, 0, &meta);

    db->pm->num_pages = 1 + num_data_pages;
    if (ftruncate(db->pm->fd, (off_t)db->pm->num_pages * PAGE_SIZE) != 0) {
        free(page_buf);
        return DB_ERROR;
    }

    free(page_buf);
    return DB_SUCCESS;
}

db_result_t db_open(const char *filename, db_t **db) {
    if (!filename || !db) {
        return DB_ERROR;
    }

    db_t *new_db = calloc(1, sizeof(struct db_impl));
    if (!new_db) {
        return DB_ERROR;
    }

    new_db->filename = strdup(filename);
    if (!new_db->filename) {
        free(new_db);
        return DB_ERROR;
    }

    new_db->index = btree_create();
    if (!new_db->index) {
        free(new_db->filename);
        free(new_db);
        return DB_ERROR;
    }

    new_db->pm = page_manager_create(filename);
    if (!new_db->pm) {
        btree_destroy(new_db->index);
        free(new_db->filename);
        free(new_db);
        return DB_ERROR;
    }
    new_db->persistent = true;
    new_db->in_transaction = false;
    new_db->undo_log = NULL;
    if (pthread_rwlock_init(&new_db->rwlock, NULL) != 0) {
        page_manager_destroy(new_db->pm);
        btree_destroy(new_db->index);
        free(new_db->filename);
        free(new_db);
        return DB_ERROR;
    }

    if (new_db->pm->num_pages > 0) {
        db_result_t load_result = db_load_from_pages(new_db);
        if (load_result != DB_SUCCESS) {
            pthread_rwlock_destroy(&new_db->rwlock);
            page_manager_destroy(new_db->pm);
            btree_destroy(new_db->index);
            free(new_db->filename);
            free(new_db);
            return load_result;
        }
    }
    if (db_replay_wal(new_db) != DB_SUCCESS) {
        pthread_rwlock_destroy(&new_db->rwlock);
        page_manager_destroy(new_db->pm);
        btree_destroy(new_db->index);
        free(new_db->filename);
        free(new_db);
        return DB_ERROR;
    }

    *db = new_db;
    return DB_SUCCESS;
}

db_result_t db_close(db_t *db) {
    if (!db) {
        return DB_ERROR;
    }

    if (db->in_transaction) {
        db_rollback(db);
    }
    if (db->persistent && db->pm && db->index) {
        db_save_to_pages(db);
        db_wal_checkpoint(db);
    }
    tx_clear(db);

    if (db->index) {
        btree_destroy(db->index);
        db->index = NULL;
    }

    if (db->pm) {
        page_manager_destroy(db->pm);
        db->pm = NULL;
    }

    pthread_rwlock_destroy(&db->rwlock);

    if (db->filename) {
        free(db->filename);
        db->filename = NULL;
    }

    free(db);
    return DB_SUCCESS;
}

db_result_t db_insert(db_t *db, const char *key, const char *value) {
    if (!db || !key || !value) {
        return DB_ERROR;
    }
    pthread_rwlock_wrlock(&db->rwlock);
    char *existing_value = btree_search(db->index, key);
    if (existing_value != NULL) {
        free(existing_value);
        pthread_rwlock_unlock(&db->rwlock);
        return DB_EXISTS;
    }
    int result = btree_insert(db->index, key, value);
    if (result != 0) {
        pthread_rwlock_unlock(&db->rwlock);
        return DB_ERROR;
    }
    if (db->in_transaction && tx_push(db, TX_OP_INSERT, key, NULL) == NULL) {
        btree_delete(db->index, key);
        pthread_rwlock_unlock(&db->rwlock);
        return DB_ERROR;
    }
    pthread_rwlock_unlock(&db->rwlock);
    return DB_SUCCESS;
}

db_result_t db_get(db_t *db, const char *key, char **value) {
    if (!db || !key || !value) {
        return DB_ERROR;
    }
    pthread_rwlock_rdlock(&db->rwlock);
    char *found_value = btree_search(db->index, key);
    if (found_value == NULL) {
        pthread_rwlock_unlock(&db->rwlock);
        return DB_NOT_FOUND;
    }
    *value = found_value;
    pthread_rwlock_unlock(&db->rwlock);
    return DB_SUCCESS;
}

db_result_t db_delete(db_t *db, const char *key) {
    if (!db || !key) {
        return DB_ERROR;
    }
    pthread_rwlock_wrlock(&db->rwlock);
    char *existing_value = btree_search(db->index, key);
    if (existing_value == NULL) {
        pthread_rwlock_unlock(&db->rwlock);
        return DB_NOT_FOUND;
    }
    int result = btree_delete(db->index, key);
    if (result != 0) {
        free(existing_value);
        pthread_rwlock_unlock(&db->rwlock);
        return DB_ERROR;
    }
    if (db->in_transaction && tx_push(db, TX_OP_DELETE, key, existing_value) == NULL) {
        free(existing_value);
        btree_insert(db->index, key, existing_value);
        pthread_rwlock_unlock(&db->rwlock);
        return DB_ERROR;
    }
    if (!db->in_transaction) {
        free(existing_value);
    }
    pthread_rwlock_unlock(&db->rwlock);
    return DB_SUCCESS;
}

db_result_t db_update(db_t *db, const char *key, const char *value) {
    if (!db || !key || !value) {
        return DB_ERROR;
    }
    pthread_rwlock_wrlock(&db->rwlock);
    char *existing_value = btree_search(db->index, key);
    if (existing_value == NULL) {
        pthread_rwlock_unlock(&db->rwlock);
        return DB_NOT_FOUND;
    }
    int result = btree_insert(db->index, key, value);
    if (result != 0) {
        free(existing_value);
        pthread_rwlock_unlock(&db->rwlock);
        return DB_ERROR;
    }
    if (db->in_transaction && tx_push(db, TX_OP_UPDATE, key, existing_value) == NULL) {
        free(existing_value);
        btree_insert(db->index, key, existing_value);
        pthread_rwlock_unlock(&db->rwlock);
        return DB_ERROR;
    }
    if (!db->in_transaction) {
        free(existing_value);
    }
    pthread_rwlock_unlock(&db->rwlock);
    return DB_SUCCESS;
}

db_result_t db_begin(db_t *db) {
    if (!db) return DB_ERROR;
    pthread_rwlock_wrlock(&db->rwlock);
    if (db->in_transaction) {
        pthread_rwlock_unlock(&db->rwlock);
        return DB_ERROR;
    }
    tx_clear(db);
    db->in_transaction = true;
    pthread_rwlock_unlock(&db->rwlock);
    return DB_SUCCESS;
}

db_result_t db_commit(db_t *db) {
    if (!db) return DB_ERROR;
    pthread_rwlock_wrlock(&db->rwlock);
    if (!db->in_transaction) {
        pthread_rwlock_unlock(&db->rwlock);
        return DB_SUCCESS;
    }
    db_result_t result = db_wal_commit(db);
    if (result != DB_SUCCESS) {
        pthread_rwlock_unlock(&db->rwlock);
        return result;
    }
    tx_clear(db);
    db->in_transaction = false;
    pthread_rwlock_unlock(&db->rwlock);
    return DB_SUCCESS;
}

db_result_t db_rollback(db_t *db) {
    if (!db) return DB_ERROR;
    pthread_rwlock_wrlock(&db->rwlock);
    if (!db->in_transaction) {
        pthread_rwlock_unlock(&db->rwlock);
        return DB_SUCCESS;
    }
    tx_rollback_apply(db);
    tx_clear(db);
    db->in_transaction = false;
    pthread_rwlock_unlock(&db->rwlock);
    return DB_SUCCESS;
}
