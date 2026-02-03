# SimpleDB - A Simple Database Engine

A lightweight, educational database engine written in C, implementing core database concepts including B-tree indexing, persistence, and basic query operations.

## Project Goals

- Implement a B-tree based key-value store
- Add persistence to disk
- Support basic CRUD operations (Create, Read, Update, Delete)
- Implement transaction support
- Add concurrency control (locking)
- Create a simple query interface

## Features

### Phase 1: Core Engine (Current)
- [x] Project structure
- [x] B-tree search implementation
- [x] Complete B-tree insert with node splitting
- [x] Root splitting and tree growth
- [x] B-tree memory management
- [x] Database API integration (insert, get, update)
- [x] Command-line interface (CLI)
- [x] Comprehensive unit tests (B-tree: 9 cases, DB API: 10 cases including concurrency)
- [x] B-tree delete operation (with borrow/merge)
- [x] Page-based storage (page manager)
- [x] Persistence to disk (save on close, load on open)

### Phase 2: Transactions
- [x] Write-ahead logging (WAL)
- [x] Transaction support (BEGIN, COMMIT, ROLLBACK)
- [x] Undo log for rollback; WAL replay on open

### Phase 3: Concurrency (Current)
- [x] Reader-writer locks (pthread_rwlock)
- [x] Multi-threaded access (concurrent insert/get test)
- [ ] Deadlock detection (optional; single lock avoids deadlock)

### Phase 4: Query Interface
- [ ] SQL-like query parser (simple)
- [ ] Range queries
- [ ] Index scanning

## Architecture

```
┌─────────────────┐
│   Query Parser  │
└────────┬────────┘
         │
┌────────▼────────┐
│  Query Engine   │
└────────┬────────┘
         │
┌────────▼────────┐
│  B-Tree Index   │
└────────┬────────┘
         │
┌────────▼────────┐
│  Page Manager   │
└────────┬────────┘
         │
┌────────▼────────┐
│  Disk Storage   │
└─────────────────┘
```

## Building

```bash
make
```

## Usage

```bash
# Start the database
./simpledb

# Or with a database file
./simpledb mydb.db
```

## Testing

```bash
# Run all tests
make test

# Run B-tree tests only
make test-btree

# Run database API tests only
make test-db
```

## Learning Resources

- Database Systems: The Complete Book (Garcia-Molina)
- SQLite Architecture: https://www.sqlite.org/arch.html
- B-Tree Visualization: https://www.cs.usfca.edu/~galles/visualization/BTree.html

## License

MIT
