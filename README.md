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
- [x] Comprehensive unit tests (B-tree: 9 cases, DB API: 6 cases)
- [x] B-tree delete operation (with borrow/merge)
- [ ] Page-based storage
- [ ] Persistence to disk

### Phase 2: Transactions
- [ ] Write-ahead logging (WAL)
- [ ] Transaction support (BEGIN, COMMIT, ROLLBACK)
- [ ] ACID properties

### Phase 3: Concurrency
- [ ] Reader-writer locks
- [ ] Multi-threaded access
- [ ] Deadlock detection

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
