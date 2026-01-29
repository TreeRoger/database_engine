#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

void print_usage(const char *program_name) {
    printf("Usage: %s [database_file]\n", program_name);
    printf("\n");
    printf("SimpleDB - A simple database engine\n");
    printf("\n");
    printf("Commands:\n");
    printf("  INSERT <key> <value>  - Insert a key-value pair\n");
    printf("  GET <key>             - Get a value by key\n");
    printf("  DELETE <key>          - Delete a key-value pair\n");
    printf("  UPDATE <key> <value>  - Update a key-value pair\n");
    printf("  QUIT                  - Exit the database\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    const char *db_file = (argc > 1) ? argv[1] : "default.db";
    db_t *db = NULL;
    
    printf("SimpleDB v0.1.0\n");
    printf("Opening database: %s\n", db_file);
    
    if (db_open(db_file, &db) != DB_SUCCESS) {
        fprintf(stderr, "Error: Failed to open database\n");
        return 1;
    }
    
    printf("Database opened successfully!\n");
    printf("Type 'QUIT' to exit\n\n");
    
    char line[1024];
    char command[32];
    char key[256];
    char value[256];
    
    while (1) {
        printf("simpledb> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (sscanf(line, "%31s", command) != 1) {
            continue;
        }
        
        if (strcmp(command, "QUIT") == 0 || strcmp(command, "EXIT") == 0) {
            break;
        } else if (strcmp(command, "INSERT") == 0) {
            if (sscanf(line, "%*s %255s %255s", key, value) == 2) {
                db_result_t result = db_insert(db, key, value);
                if (result == DB_SUCCESS) {
                    printf("OK: Inserted key '%s'\n", key);
                } else if (result == DB_EXISTS) {
                    printf("Error: Key '%s' already exists\n", key);
                } else {
                    printf("Error: Insert failed\n");
                }
            } else {
                printf("Usage: INSERT <key> <value>\n");
            }
        } else if (strcmp(command, "GET") == 0) {
            if (sscanf(line, "%*s %255s", key) == 1) {
                char *value_ptr = NULL;
                db_result_t result = db_get(db, key, &value_ptr);
                if (result == DB_SUCCESS && value_ptr) {
                    printf("Value: %s\n", value_ptr);
                    free(value_ptr);
                } else {
                    printf("Error: Key '%s' not found\n", key);
                }
            } else {
                printf("Usage: GET <key>\n");
            }
        } else if (strcmp(command, "DELETE") == 0) {
            if (sscanf(line, "%*s %255s", key) == 1) {
                db_result_t result = db_delete(db, key);
                if (result == DB_SUCCESS) {
                    printf("OK: Deleted key '%s'\n", key);
                } else {
                    printf("Error: Key '%s' not found\n", key);
                }
            } else {
                printf("Usage: DELETE <key>\n");
            }
        } else if (strcmp(command, "UPDATE") == 0) {
            if (sscanf(line, "%*s %255s %255s", key, value) == 2) {
                db_result_t result = db_update(db, key, value);
                if (result == DB_SUCCESS) {
                    printf("OK: Updated key '%s'\n", key);
                } else {
                    printf("Error: Key '%s' not found\n", key);
                }
            } else {
                printf("Usage: UPDATE <key> <value>\n");
            }
        } else {
            printf("Unknown command: %s\n", command);
            print_usage(argv[0]);
        }
    }
    
    db_close(db);
    printf("\nGoodbye!\n");
    return 0;
}
