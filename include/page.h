#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdbool.h>

// Page size (4KB, typical for databases)
#define PAGE_SIZE 4096

// Page types
typedef enum {
    PAGE_TYPE_METADATA = 0,
    PAGE_TYPE_BTREE = 1,
    PAGE_TYPE_DATA = 2,
    PAGE_TYPE_FREE = 3,
} page_type_t;

// Page header structure
typedef struct {
    page_type_t type;
    uint32_t page_id;
    uint32_t next_page;
    uint32_t data_size;
    uint8_t data[PAGE_SIZE - 16];  // Remaining space for data
} page_t;

// Page manager
typedef struct {
    int fd;
    uint32_t num_pages;
    uint32_t free_page_list;
} page_manager_t;

// Initialize page manager
page_manager_t* page_manager_create(const char *filename);

// Close page manager
void page_manager_destroy(page_manager_t *pm);

// Allocate a new page
uint32_t page_alloc(page_manager_t *pm);

// Free a page
void page_free(page_manager_t *pm, uint32_t page_id);

// Read a page from disk
int page_read(page_manager_t *pm, uint32_t page_id, page_t *page);

// Write a page to disk
int page_write(page_manager_t *pm, uint32_t page_id, page_t *page);

#endif // PAGE_H
