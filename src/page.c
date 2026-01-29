#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "page.h"

page_manager_t* page_manager_create(const char *filename) {
    if (!filename) {
        return NULL;
    }
    
    page_manager_t *pm = calloc(1, sizeof(page_manager_t));
    if (!pm) {
        return NULL;
    }
    
    // Open or create the database file
    pm->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (pm->fd < 0) {
        free(pm);
        return NULL;
    }
    
    // Check if file is new (empty)
    struct stat st;
    if (fstat(pm->fd, &st) == 0 && st.st_size == 0) {
        // Initialize new database file
        // Write metadata page (page 0)
        page_t metadata;
        memset(&metadata, 0, sizeof(page_t));
        metadata.type = PAGE_TYPE_METADATA;
        metadata.page_id = 0;
        
        if (write(pm->fd, &metadata, PAGE_SIZE) != PAGE_SIZE) {
            close(pm->fd);
            free(pm);
            return NULL;
        }
        
        pm->num_pages = 1;
        pm->free_page_list = 0;
    } else {
        // TODO: Read metadata from existing file
        pm->num_pages = st.st_size / PAGE_SIZE;
    }
    
    return pm;
}

void page_manager_destroy(page_manager_t *pm) {
    if (!pm) {
        return;
    }
    
    if (pm->fd >= 0) {
        fsync(pm->fd);  // Ensure all data is written
        close(pm->fd);
    }
    
    free(pm);
}

uint32_t page_alloc(page_manager_t *pm) {
    if (!pm) {
        return 0;
    }
    
    // TODO: Implement page allocation
    // 1. Check free page list first
    // 2. If no free pages, allocate new page at end
    // 3. Update metadata
    
    uint32_t page_id = pm->num_pages;
    pm->num_pages++;
    return page_id;
}

void page_free(page_manager_t *pm, uint32_t page_id) {
    if (!pm || page_id == 0) {
        return;  // Can't free metadata page
    }
    
    // TODO: Add page to free list
    // Mark page as free type
}

int page_read(page_manager_t *pm, uint32_t page_id, page_t *page) {
    if (!pm || !page || page_id >= pm->num_pages) {
        return -1;
    }
    
    off_t offset = page_id * PAGE_SIZE;
    if (lseek(pm->fd, offset, SEEK_SET) != offset) {
        return -1;
    }
    
    if (read(pm->fd, page, PAGE_SIZE) != PAGE_SIZE) {
        return -1;
    }
    
    return 0;
}

int page_write(page_manager_t *pm, uint32_t page_id, page_t *page) {
    if (!pm || !page || page_id >= pm->num_pages) {
        return -1;
    }
    
    page->page_id = page_id;
    
    off_t offset = page_id * PAGE_SIZE;
    if (lseek(pm->fd, offset, SEEK_SET) != offset) {
        return -1;
    }
    
    if (write(pm->fd, page, PAGE_SIZE) != PAGE_SIZE) {
        return -1;
    }
    
    return 0;
}
