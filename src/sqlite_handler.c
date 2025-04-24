#include "sqlite_handler.h"
#include <regex.h>
#include "server.h"
#include "debug.h"

static sqlite3* db = NULL;
static char* db_path = NULL;
static bool db_initialized = false;

int init_sqlite(const char* path) {
    #ifdef DEBUG_MODE
    printf("%s%s[DEBUG] %sinit_sqlite() called with path: %s%s\n", 
           BOLD, COLOR_CYAN, COLOR_RESET, 
           path ? path : "NULL", COLOR_RESET);
    #endif
    
    if (db != NULL) {
        close_sqlite();
    }

    if (db_path != NULL) {
        free(db_path);
        db_path = NULL;
    }

    if (path == NULL || *path == '\0') {
        printf("%s%s[SQLite] %s%sNo database path provided%s\n", 
               BOLD, COLOR_RED, BOLD, COLOR_RESET, COLOR_RESET);
        return -1;
    }

    db_path = strdup(path);
    if (db_path == NULL) {
        printf("%s%s[SQLite] %s%sMemory allocation error%s\n", 
               BOLD, COLOR_RED, BOLD, COLOR_RESET, COLOR_RESET);
        return -1;
    }

    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        printf("%s%s[SQLite] %s%sCannot open database: %s%s\n", 
               BOLD, COLOR_RED, BOLD, COLOR_RESET, sqlite3_errmsg(db), COLOR_RESET);
        sqlite3_close(db);
        db = NULL;
        return -1;
    }

    printf("%s%s[SQLite] %sDatabase initialized: %s%s%s\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, COLOR_CYAN, path, COLOR_RESET);
    db_initialized = true;
    
    #ifdef DEBUG_MODE
    printf("%s%s[DEBUG] %sAfter init_sqlite(): db_initialized=%s%s\n", 
           BOLD, COLOR_CYAN, COLOR_RESET, 
           db_initialized ? "true" : "false", COLOR_RESET);
    #endif
    
    return 0;
}

void close_sqlite() {
    #ifdef DEBUG_MODE
    printf("%s%s[DEBUG] %sclose_sqlite() called, db=%p, db_initialized=%s%s\n", 
           BOLD, COLOR_CYAN, COLOR_RESET, (void*)db, 
           db_initialized ? "true" : "false", COLOR_RESET);
    #endif
    
    if (db != NULL) {
        sqlite3_close(db);
        db = NULL;
        printf("%s%s[SQLite] %sDatabase connection closed%s\n", 
               BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
    }
    
    if (db_path != NULL) {
        free(db_path);
        db_path = NULL;
    }
    
    db_initialized = false;
    
    #ifdef DEBUG_MODE
    printf("%s%s[DEBUG] %sAfter close_sqlite(): db_initialized=%s%s\n", 
           BOLD, COLOR_CYAN, COLOR_RESET, 
           db_initialized ? "true" : "false", COLOR_RESET);
    #endif
}

static int callback(void* data, int argc, char** argv, char** azColName) {
    sqlite_result_t* result = (sqlite_result_t*)data;
    if (result->column_count == 0) {
        result->column_count = argc;
        result->columns = malloc(argc * sizeof(char*));
        if (!result->columns) {
            fprintf(stderr, "%s%s[SQLite] %sMemory allocation error for columns%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
            return 1;
        }
        
        for (int i = 0; i < argc; i++) {
            result->columns[i] = strdup(azColName[i] ? azColName[i] : "");
        }
    }
    
    if (result->row_count >= result->capacity) {
        int new_capacity = result->capacity == 0 ? 10 : result->capacity * 2;
        char*** new_rows = realloc(result->rows, new_capacity * sizeof(char**));
        if (!new_rows) {
            fprintf(stderr, "%s%s[SQLite] %sMemory allocation error for rows%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
            return 1;
        }
        result->rows = new_rows;
        result->capacity = new_capacity;
    }
    
    result->rows[result->row_count] = malloc(argc * sizeof(char*));
    if (!result->rows[result->row_count]) {
        fprintf(stderr, "%s%s[SQLite] %sMemory allocation error for row %d%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, result->row_count, COLOR_RESET);
        return 1;
    }
    
    for (int i = 0; i < argc; i++) {
        result->rows[result->row_count][i] = strdup(argv[i] ? argv[i] : "");
    }
    
    result->row_count++;
    return 0;
}

sqlite_result_t* execute_query(const char* query) {
    if (!db_initialized || db == NULL) {
        fprintf(stderr, "%s%s[SQLite] %sDatabase not initialized%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        return NULL;
    }
    
    sqlite_result_t* result = malloc(sizeof(sqlite_result_t));
    if (!result) {
        fprintf(stderr, "%s%s[SQLite] %sMemory allocation error for result%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        return NULL;
    }
    
    memset(result, 0, sizeof(sqlite_result_t));
    
    char* err_msg = NULL;
    printf("%s%s[SQLite] %sExecuting query: %s%s%s\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, COLOR_CYAN, query, COLOR_RESET);
    
    int rc = sqlite3_exec(db, query, callback, result, &err_msg);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s%s[SQLite] %s%sSQL error: %s%s\n", 
                BOLD, COLOR_RED, BOLD, COLOR_RESET, err_msg, COLOR_RESET);
        sqlite3_free(err_msg);
        free_query_results(result);
        return NULL;
    }
    
    return result;
}

void free_query_results(sqlite_result_t* results) {
    if (results == NULL) {
        return;
    }
    
    if (results->columns) {
        for (int i = 0; i < results->column_count; i++) {
            if (results->columns[i]) {
                free(results->columns[i]);
            }
        }
        free(results->columns);
    }
    
    if (results->rows) {
        for (int i = 0; i < results->row_count; i++) {
            if (results->rows[i]) {
                for (int j = 0; j < results->column_count; j++) {
                    if (results->rows[i][j]) {
                        free(results->rows[i][j]);
                    }
                }
                free(results->rows[i]);
            }
        }
        free(results->rows);
    }
    
    free(results);
}

bool is_db_initialized() {
    return db_initialized;
}

const char* get_db_path() {
    return db_path;
}

void set_db_path(const char* path) {
    if (db_path != NULL) {
        free(db_path);
        db_path = NULL;
    }
    
    if (path != NULL) {
        db_path = strdup(path);
        printf("%s%s[SQLite] %sDatabase path set to: %s%s%s\n", 
               BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_CYAN, db_path, COLOR_RESET);
    } else {
        printf("%s%s[SQLite] %sDatabase path cleared%s\n", 
               BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_RESET);
        if (db != NULL) {
            close_sqlite();
        }
    }
}

char* generate_table_html(sqlite_result_t* result) {
    if (!result) {
        return strdup("<p>No query results.</p>");
    }
    
    char* html = NULL;
    size_t size = 0;
    FILE* memfile = open_memstream(&html, &size);
    
    if (!memfile) {
        return strdup("<p>Error generating results table.</p>");
    }
    
    fprintf(memfile, "<table class=\"sql-table\">\n");
    fprintf(memfile, "  <thead>\n    <tr>\n");
    for (int i = 0; i < result->column_count; i++) {
        fprintf(memfile, "      <th>%s</th>\n", result->columns[i] ? result->columns[i] : "");
    }
    fprintf(memfile, "    </tr>\n  </thead>\n");
    fprintf(memfile, "  <tbody>\n");
    
    if (result->row_count == 0) {
        fprintf(memfile, "    <tr><td colspan=\"%d\">No results found</td></tr>\n", 
                result->column_count > 0 ? result->column_count : 1);
    } else {
        for (int i = 0; i < result->row_count; i++) {
            fprintf(memfile, "    <tr>\n");
            for (int j = 0; j < result->column_count; j++) {
                fprintf(memfile, "      <td>%s</td>\n", 
                        result->rows[i][j] ? result->rows[i][j] : "");
            }
            fprintf(memfile, "    </tr>\n");
        }
    }
    
    fprintf(memfile, "  </tbody>\n</table>\n");
    
    fclose(memfile);
    return html;
}

char* process_sqlite_queries(char* content) {
    if (!content || !is_db_initialized()) {
        return content;
    }
    
    const char* query_pattern = "\\{\\% query \"([^\"]*)\" \\%\\}";
    regex_t regex;
    
    if (regcomp(&regex, query_pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "%s%s[SQLite] %sRegex compilation failed%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        return content;
    }
    
    size_t content_len = strlen(content);
    size_t result_capacity = content_len * 2;
    char* result = malloc(result_capacity + 1);
    
    if (!result) {
        fprintf(stderr, "%s%s[SQLite] %sMemory allocation error%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        regfree(&regex);
        return content;
    }
    
    size_t result_len = 0;
    size_t last_pos = 0;
    regmatch_t matches[2];
    
    while (regexec(&regex, content + last_pos, 2, matches, 0) == 0) {
        size_t start_offset = last_pos + matches[0].rm_so;
        size_t end_offset = last_pos + matches[0].rm_eo;
        
        size_t prefix_len = start_offset - last_pos;
        if (prefix_len > 0) {
            if (result_len + prefix_len >= result_capacity) {
                result_capacity = (result_len + prefix_len) * 2;
                char* new_result = realloc(result, result_capacity + 1);
                if (!new_result) {
                    fprintf(stderr, "%s%s[SQLite] %sMemory reallocation error%s\n", 
                            BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
                    free(result);
                    regfree(&regex);
                    return content;
                }
                result = new_result;
            }
            
            memcpy(result + result_len, content + last_pos, prefix_len);
            result_len += prefix_len;
        }
        
        size_t query_start = last_pos + matches[1].rm_so;
        size_t query_end = last_pos + matches[1].rm_eo;
        size_t query_len = query_end - query_start;
        
        char* query = malloc(query_len + 1);
        if (!query) {
            fprintf(stderr, "%s%s[SQLite] %sMemory allocation error%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
            free(result);
            regfree(&regex);
            return content;
        }
        
        memcpy(query, content + query_start, query_len);
        query[query_len] = '\0';
        sqlite_result_t* query_result = execute_query(query);
        free(query);
        char* table_html = generate_table_html(query_result);
        if (query_result) {
            free_query_results(query_result);
        }
        
        size_t html_len = strlen(table_html);
        if (result_len + html_len >= result_capacity) {
            result_capacity = (result_len + html_len) * 2;
            char* new_result = realloc(result, result_capacity + 1);
            if (!new_result) {
                fprintf(stderr, "%s%s[SQLite] %sMemory reallocation error%s\n", 
                        BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
                free(table_html);
                free(result);
                regfree(&regex);
                return content;
            }
            result = new_result;
        }
        

        memcpy(result + result_len, table_html, html_len);
        result_len += html_len;
        free(table_html);
        last_pos = end_offset;
    }
    
    if (last_pos < content_len) {
        size_t remaining_len = content_len - last_pos;
        
        if (result_len + remaining_len >= result_capacity) {
            result_capacity = (result_len + remaining_len) * 2;
            char* new_result = realloc(result, result_capacity + 1);
            if (!new_result) {
                fprintf(stderr, "%s%s[SQLite] %sMemory reallocation error%s\n", 
                        BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
                free(result);
                regfree(&regex);
                return content;
            }
            result = new_result;
        }
        
        memcpy(result + result_len, content + last_pos, remaining_len);
        result_len += remaining_len;
    }
    result[result_len] = '\0';
    regfree(&regex);
    return result;
} 