#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include "db.h"
#include "btree.h"
#include "page.h"

// Database structure implementation
struct db_impl {
    char *filename;
    page_manager_t *pm;
    btree_t *index;
    bool persistent;
};

// Metadata in page 0: first 4 bytes of data = number of data pages (1-based)
#define METADATA_NUM_PAGES_OFFSET 0
#define PAGE_DATA_CAPACITY (PAGE_SIZE - 16 - 4)  // data[] size minus 4-byte bytes_used
#define MAX_KEY_LEN 4096
#define MAX_VALUE_LEN 65536

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

    if (new_db->pm->num_pages > 0) {
        db_result_t load_result = db_load_from_pages(new_db);
        if (load_result != DB_SUCCESS) {
            page_manager_destroy(new_db->pm);
            btree_destroy(new_db->index);
            free(new_db->filename);
            free(new_db);
            return load_result;
        }
    }

    *db = new_db;
    return DB_SUCCESS;
}

db_result_t db_close(db_t *db) {
    if (!db) {
        return DB_ERROR;
    }

    if (db->persistent && db->pm && db->index) {
        db_save_to_pages(db);
    }

    if (db->index) {
        btree_destroy(db->index);
        db->index = NULL;
    }

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
