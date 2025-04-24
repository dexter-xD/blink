#ifndef SQLITE_HANDLER_H
#define SQLITE_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    char*** rows;       
    char** columns;     
    int row_count;      
    int column_count;   
    int capacity;       
} sqlite_result_t;

int init_sqlite(const char* db_path);
void close_sqlite();
sqlite_result_t* execute_query(const char* query);
void free_query_results(sqlite_result_t* results);
bool is_db_initialized();
const char* get_db_path();
void set_db_path(const char* path);
char* process_sqlite_queries(char* content);
char* generate_table_html(sqlite_result_t* result);

#endif 