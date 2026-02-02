# Implementation Guide

This guide will help you implement the database engine step by step.

## Phase 1: Core B-Tree Implementation

### Step 1: Complete B-Tree Search
The search operation is the foundation. Start here:

```c
char* btree_search_node(btree_node_t *node, const char *key) {
    int i = 0;
    // Find the position where key should be
    while (i < node->num_keys && strcmp(key, node->keys[i]) > 0) {
        i++;
    }
    
    if (i < node->num_keys && strcmp(key, node->keys[i]) == 0) {
        // Found it!
        return strdup(node->values[i]);
    }
    
    if (node->is_leaf) {
        return NULL;  // Not found
    }
    
    // Recurse into child
    return btree_search_node(node->children[i], key);
}
```

### Step 2: Implement B-Tree Insert
This is more complex. Key steps:

1. **Split full nodes**: If a node has `2*BTREE_ORDER - 1` keys, split it
2. **Insert into leaf**: Find the correct leaf and insert
3. **Handle splits**: Propagate splits upward if needed

**Algorithm:**
- Start from root
- If root is full, split it (increases tree height)
- Traverse down, splitting full nodes as you go
- Insert into the appropriate leaf

### Step 3: Implement B-Tree Delete
Most complex operation. Cases to handle:

1. **Key in leaf**: Simply remove it
2. **Key in internal node**: 
   - Replace with predecessor/successor
   - Recursively delete from leaf
3. **Node underflow**: Merge with sibling or borrow from sibling

## Phase 2: Page Management

### Step 1: Complete Page Allocation
- Maintain a free page list in metadata
- When allocating, check free list first
- If empty, extend the file

### Step 2: Serialize B-Tree to Pages
- Each B-tree node becomes one or more pages
- Store node data in page->data
- Maintain page IDs for child pointers

### Step 3: Load B-Tree from Disk
- Read root page ID from metadata
- Recursively load nodes from pages
- Reconstruct B-tree in memory

## Phase 3: Integration

### Connect Everything
1. `db_insert()` → `btree_insert()` → `page_write()`
2. `db_get()` → `btree_search()` → `page_read()` (if values stored separately)
3. `db_delete()` → `btree_delete()` → `page_free()`

## Testing Strategy

### Unit Tests
1. Test B-tree operations in isolation (no disk)
2. Test page manager separately
3. Test each operation individually

### Integration Tests
1. Insert → Get (should work)
2. Insert → Delete → Get (should fail)
3. Insert many keys, verify all can be retrieved
4. Close and reopen database, verify persistence

### Stress Tests
1. Insert 10,000 keys
2. Delete random keys
3. Verify integrity

## Learning Resources

### B-Tree Algorithm
- **CLRS Chapter 18**: B-Trees (Introduction to Algorithms)
- **Visualization**: https://www.cs.usfca.edu/~galles/visualization/BTree.html
- Key invariant: Every node (except root) has at least `t-1` keys, at most `2t-1` keys

### Database Internals
- **SQLite Architecture**: https://www.sqlite.org/arch.html
- **PostgreSQL Internals**: Great for understanding page-based storage
- **Database Systems: The Complete Book** by Garcia-Molina

## Milestones

### Week 1-2: B-Tree Core
- [ ] Implement search
- [ ] Implement insert
- [ ] Implement delete
- [ ] Unit tests for B-tree

### Week 3: Persistence
- [ ] Complete page manager
- [ ] Serialize B-tree to disk
- [ ] Load B-tree from disk
- [ ] Integration tests

### Week 4: Polish
- [ ] Error handling
- [ ] Memory leak fixes
- [ ] Performance optimization
- [ ] Documentation

### Week 5+: Advanced Features
- [ ] Transactions (WAL)
- [ ] Concurrency (locks)
- [ ] Query interface

## Tips

1. **Start Simple**: Get search working first, then insert, then delete
2. **Test Incrementally**: After each function, write a test
3. **Use Valgrind**: Check for memory leaks regularly
4. **Visualize**: Print the tree structure to debug
5. **Reference Implementation**: Look at SQLite's B-tree code (it's well-documented)

## Common Pitfalls

1. **Off-by-one errors**: B-tree order calculations are tricky
2. **Memory management**: Don't forget to free strings when deleting
3. **File I/O**: Always check return values, handle partial writes
4. **Node splitting**: Make sure parent pointers are updated correctly
5. **Edge cases**: Empty tree, single node, root split

Good luck!
