CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDFLAGS = -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = build
TEST_DIR = tests

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Target executable
TARGET = simpledb

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Create build directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Test executables
TEST_BTREE_TARGET = test_btree
TEST_DB_TARGET = test_db
TEST_BTREE_OBJECTS = $(OBJ_DIR)/test_btree.o $(OBJ_DIR)/btree.o
TEST_DB_OBJECTS = $(OBJ_DIR)/test_db.o $(OBJ_DIR)/db.o $(OBJ_DIR)/btree.o $(OBJ_DIR)/page.o

# Build all tests
test: test-btree test-db

# Build and run B-tree tests
test-btree: $(TEST_BTREE_TARGET)
	./$(TEST_BTREE_TARGET)

# Build and run database API tests
test-db: $(TEST_DB_TARGET)
	./$(TEST_DB_TARGET)

$(TEST_BTREE_TARGET): $(TEST_BTREE_OBJECTS)
	$(CC) $(TEST_BTREE_OBJECTS) -o $(TEST_BTREE_TARGET) $(LDFLAGS)

$(TEST_DB_TARGET): $(TEST_DB_OBJECTS)
	$(CC) $(TEST_DB_OBJECTS) -o $(TEST_DB_TARGET) $(LDFLAGS)

$(OBJ_DIR)/test_btree.o: $(TEST_DIR)/test_btree.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR)/test_db.o: $(TEST_DIR)/test_db.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST_BTREE_TARGET) $(TEST_DB_TARGET) *.db

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all test clean install
