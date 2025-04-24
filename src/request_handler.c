#include "request_handler.h"
#include "websocket.h"
#include "sqlite_handler.h"

bool enable_templates = true;
char* custom_html_file = NULL;
int server_port = PORT;
const char* default_html_file = "index.html";

char* parse_form_data(const char* buffer, size_t buffer_size) {
    const char* body_start = strstr(buffer, "\r\n\r\n");
    if (!body_start) {
        return NULL;
    }
    
    body_start += 4;
    size_t body_length = buffer_size - (body_start - buffer);
    if (body_length <= 0) {
        return NULL;
    }
 
    char* form_data = malloc(body_length + 1);
    if (!form_data) {
        return NULL;
    }
    
    memcpy(form_data, body_start, body_length);
    form_data[body_length] = '\0';
    
    return form_data;
}

char* get_form_value(const char* form_data, const char* key) {
    if (!form_data || !key) {
        return NULL;
    }
    
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "%s=", key);
    
    const char* value_start = strstr(form_data, search_key);
    if (!value_start) {
        return NULL;
    }
    
    value_start += strlen(search_key);
    
    const char* value_end = strchr(value_start, '&');
    if (!value_end) {
        value_end = value_start + strlen(value_start);
    }
    
    size_t value_length = value_end - value_start;
    char* value = malloc(value_length + 1);
    if (!value) {
        return NULL;
    }
    
    strncpy(value, value_start, value_length);
    value[value_length] = '\0';
    
    char* decoded = malloc(value_length + 1);
    if (!decoded) {
        free(value);
        return NULL;
    }
    
    size_t i, j = 0;
    for (i = 0; i < value_length; i++) {
        if (value[i] == '%' && i + 2 < value_length) {
            int hex_val;
            sscanf(value + i + 1, "%2x", &hex_val);
            decoded[j++] = (char)hex_val;
            i += 2;
        } else if (value[i] == '+') {
            decoded[j++] = ' ';
        } else {
            decoded[j++] = value[i];
        }
    }
    decoded[j] = '\0';
    
    free(value);
    return decoded;
}

char* handle_sql_form(const char* form_data) {
    if (!form_data || !is_db_initialized()) {
        return strdup("<p>Error: Form data missing or database not initialized</p>");
    }
    
    char* sql_action = get_form_value(form_data, "sql_action");
    if (!sql_action) {
        return strdup("<p>Error: Missing SQL action in form</p>");
    }
    
    char* sql_query_template = get_form_value(form_data, "sql_query");
    char* result_html = NULL;
    
    if (sql_query_template) {
        char* sql_query = strdup(sql_query_template);
        if (!sql_query) {
            free(sql_action);
            free(sql_query_template);
            return strdup("<div class=\"sql-error\"><p>Memory allocation error</p></div>");
        }
        
        const char* form_ptr = form_data;
        while (*form_ptr) {
            const char* name_start = form_ptr;
            const char* equals = strchr(name_start, '=');
            if (!equals) break;
            
            size_t name_len = equals - name_start;
            if (name_len > 0 && name_len < 256) {
                char field_name[256];
                strncpy(field_name, name_start, name_len);
                field_name[name_len] = '\0';
                
                if (strcmp(field_name, "sql_action") != 0 && strcmp(field_name, "sql_query") != 0) {
                    
                    char* field_value = get_form_value(form_data, field_name);
                    if (field_value) {

                        char placeholder[300];
                        snprintf(placeholder, sizeof(placeholder), "[%s]", field_name);
                        char* pos = strstr(sql_query, placeholder);
                        while (pos) {
                            bool is_numeric = false;
                            if (strcmp(field_name, "age") == 0 || strcmp(field_name, "id") == 0) {
                                is_numeric = true;
                            }
                            
                            size_t placeholder_len = strlen(placeholder);
                            size_t value_len = strlen(field_value);
                            size_t prefix_len = pos - sql_query;
                            size_t suffix_len = strlen(pos + placeholder_len);
                            
                            size_t new_len = prefix_len + value_len + suffix_len + 1;
                            char* new_query = malloc(new_len);
                            if (!new_query) {
                                free(field_value);
                                free(sql_query);
                                free(sql_action);
                                free(sql_query_template);
                                return strdup("<div class=\"sql-error\"><p>Memory allocation error</p></div>");
                            }
                            
                            memcpy(new_query, sql_query, prefix_len);
                            memcpy(new_query + prefix_len, field_value, value_len);
                            memcpy(new_query + prefix_len + value_len, pos + placeholder_len, suffix_len + 1);
                            free(sql_query);
                            sql_query = new_query;
                            pos = strstr(sql_query, placeholder);
                        }
                        
                        free(field_value);
                    }
                }
            }
            
            form_ptr = strchr(equals + 1, '&');
            if (!form_ptr) break;
            form_ptr++;
        }
        
        printf("%s%s[SQLite] %sExecuting form %s query: %s%s\n", 
               BOLD, COLOR_BLUE, COLOR_RESET, sql_action, COLOR_CYAN, sql_query);
        
        sqlite_result_t* result = execute_query(sql_query);
        if (result) {
            if (strncasecmp(sql_query, "SELECT", 6) == 0) {
                char* table_html = generate_table_html(result);
                size_t result_len = strlen(table_html) + 256;
                result_html = malloc(result_len);
                if (result_html) {
                    snprintf(result_html, result_len, 
                             "<div class=\"sql-success\"><p>Query executed successfully!</p>%s</div>", 
                             table_html);
                }
                free(table_html);
            } else {
                int affected_rows = result->row_count;
                size_t result_len = 256;
                result_html = malloc(result_len);
                if (result_html) {
                    snprintf(result_html, result_len, 
                             "<div class=\"sql-success\"><p>Query executed successfully! "
                             "Operation affected %d record(s).</p></div>", 
                             affected_rows);
                }
            }
            
            free_query_results(result);
        } else {
            result_html = strdup("<div class=\"sql-error\"><p>Error executing SQL query.</p></div>");
        }
        
        free(sql_query);
        free(sql_query_template);
    } else {
        result_html = strdup("<div class=\"sql-error\"><p>Missing SQL query in form data.</p></div>");
    }
    
    free(sql_action);
    return result_html ? result_html : strdup("<p>Error processing form</p>");
}

char* extract_method(const char* request) {
    static char method[16];
    
    char* end_of_first_line = strstr(request, "\r\n");
    if (!end_of_first_line) {
        return NULL;
    }
    
    size_t line_len = end_of_first_line - request;
    char first_line[512];
    if (line_len >= sizeof(first_line) - 1) {
        return NULL;  
    }
    
    strncpy(first_line, request, line_len);
    first_line[line_len] = '\0';
    
    if (sscanf(first_line, "%15s", method) != 1) {
        return NULL;
    }
    
    return method;
}

void set_server_port(int port) {
    server_port = port;
    printf("%s%s[CONFIG] %sServer port set to: %d%s\n", 
           BOLD, COLOR_YELLOW, COLOR_RESET, port, COLOR_RESET);
}

void set_custom_html_file(const char* file_path) {
    if (file_path && *file_path) {
        if (custom_html_file) {
            free(custom_html_file);
        }
        
        custom_html_file = strdup(file_path);
        
        printf("%s%s[CONFIG] %sCustom HTML file path set to: %s%s\n", 
               BOLD, COLOR_YELLOW, COLOR_RESET, custom_html_file, COLOR_RESET);
    }
}

bool has_template_features(const char* content) {
    if (!content) return false;
    
    if (strstr(content, "{{") && strstr(content, "}}"))
        return true;
        
    if (strstr(content, "{% if ") && strstr(content, "{% endif %}"))
        return true;
        
    if (strstr(content, "{% for ") && strstr(content, "{% endfor %}"))
        return true;
        
    if (strstr(content, "<!-- template:var") || strstr(content, "<!-- template:items"))
        return true;
    
    if (is_db_initialized() && strstr(content, "{% query "))
        return true;
        
    return false;
}

int is_websocket_request(const char* buffer) {
    return (strstr(buffer, "Upgrade: websocket") && strstr(buffer, "Connection: Upgrade"));
}

char* extract_path(const char* request) {
    static char path[256];
    
    char* end_of_first_line = strstr(request, "\r\n");
    if (!end_of_first_line) {
        return NULL;
    }
    
    size_t line_len = end_of_first_line - request;
    char first_line[512];
    if (line_len >= sizeof(first_line) - 1) {
        return NULL;  
    }
    
    strncpy(first_line, request, line_len);
    first_line[line_len] = '\0';
    
    char method[16], http_version[16];
    if (sscanf(first_line, "%15s %255s %15s", method, path, http_version) != 3) {
        return NULL;
    }
    
    return path;
}

void handle_client(int new_socket) {
    char buffer[BUFFER_SIZE * 4] = { 0 };
    ssize_t bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        perror("Error reading from client socket");
        close(new_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    
    if (is_websocket_request(buffer) && 
        (strstr(buffer, "GET /ws") || strstr(buffer, "GET /socket"))) {
        printf("%s%s[WebSocket] %sRequest detected, redirecting to WebSocket handler%s\n",
               BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
        handle_websocket_client(new_socket, ws_clients);
        return;
    }
    
    char* method = extract_method(buffer);
    char* path = extract_path(buffer);
    
    if (method && path) {
        printf("%s%s[HTTP] %s%s request: %s%s\n", 
               BOLD, COLOR_GREEN, COLOR_RESET, method, path, COLOR_RESET);
    }
    
    if (path && strcmp(path, "/favicon.ico") == 0) {
        close(new_socket);
        return;
    }
    
    bool is_sql_form = false;
    char* form_result_html = NULL;
    
    if (method && strcmp(method, "POST") == 0 && 
        path && strcmp(path, "/sql") == 0) {
        
        printf("%s%s[SQLite] %sReceived SQL form submission%s\n", 
               BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
        
        char* form_data = parse_form_data(buffer, bytes_read);
        if (form_data) {
            is_sql_form = true;
            form_result_html = handle_sql_form(form_data);
            free(form_data);
            
            char* referer = NULL;
            const char* referer_header = strstr(buffer, "Referer: ");
            if (referer_header) {
                referer_header += 9;
                const char* referer_end = strstr(referer_header, "\r\n");
                if (referer_end) {
                    size_t referer_len = referer_end - referer_header;
                    referer = malloc(referer_len + 1);
                    if (referer) {
                        memcpy(referer, referer_header, referer_len);
                        referer[referer_len] = '\0';
                    }
                }
            }
            
            char redirect_path[256] = "/";
            if (referer) {
                const char* protocol_end = strstr(referer, "://");
                if (protocol_end) {
                    const char* host_start = protocol_end + 3;
                    const char* path_start = strchr(host_start, '/');
                    if (path_start) {
                        strncpy(redirect_path, path_start, sizeof(redirect_path) - 1);
                        redirect_path[sizeof(redirect_path) - 1] = '\0';
                    }
                }
                free(referer);
            }
            
            static char* stored_form_result = NULL;
            if (stored_form_result) {
                free(stored_form_result);
            }
            stored_form_result = form_result_html;
            
            char redirect_response[512];
            snprintf(redirect_response, sizeof(redirect_response),
                     "HTTP/1.1 302 Found\r\n"
                     "Location: %s\r\n"
                     "Content-Length: 0\r\n"
                     "Connection: close\r\n"
                     "\r\n", redirect_path);
            
            write(new_socket, redirect_response, strlen(redirect_response));
            close(new_socket);
            return;
        }
    }

    char html_file[256] = "index.html";
    
    if (custom_html_file) {
        FILE* test_file = fopen(custom_html_file, "r");
        if (test_file) {
            fclose(test_file);
            char* filename = strrchr(custom_html_file, '/');
            if (filename) {
                filename++; 
                strncpy(html_file, filename, sizeof(html_file) - 1);
            } else {
                strncpy(html_file, custom_html_file, sizeof(html_file) - 1);
            }
            
            printf("%s%s[HTTP] %sUsing custom HTML file: %s%s%s\n", 
                   BOLD, COLOR_GREEN, COLOR_RESET, COLOR_CYAN, html_file, COLOR_RESET);
        } else {
            printf("%s%s[HTTP] %s%sCustom HTML file not found: %s, falling back to index.html%s\n", 
                   BOLD, COLOR_RED, BOLD, COLOR_RESET, custom_html_file, COLOR_RESET);
        }
    } else {
        if (strcmp(path, "/advanced") == 0) {
            strcpy(html_file, "advanced.html");
        } else if (strcmp(path, "/") != 0 && strncmp(path, "/index", 6) != 0) {
            char potential_file[256];
            snprintf(potential_file, sizeof(potential_file), "%.*s.html", (int)strcspn(path + 1, "?#"), path + 1);
            
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", HTML_DIR, potential_file);
            
            FILE* test_file = fopen(full_path, "r");
            if (test_file) {
                fclose(test_file);
                strcpy(html_file, potential_file);
            }
        }
    }

    char file_path[512];
    if (custom_html_file) {
        if (custom_html_file[0] == '/') {
            strncpy(file_path, custom_html_file, sizeof(file_path) - 1);
        } 
        else {
            strncpy(file_path, custom_html_file, sizeof(file_path) - 1);
        }
        file_path[sizeof(file_path) - 1] = '\0';
    } else {
        snprintf(file_path, sizeof(file_path), "%s/%s", HTML_DIR, html_file);
    }
    
    printf("%s%s[HTTP] %sServing HTML file: %s%s%s\n", 
           BOLD, COLOR_GREEN, COLOR_RESET, COLOR_CYAN, file_path, COLOR_RESET);
    
    char* html_content = serve_html(file_path);
    if (!html_content) {
        const char* not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
        write(new_socket, not_found_response, strlen(not_found_response));
        close(new_socket);
        return;
    }

    bool should_process_templates = enable_templates && has_template_features(html_content);
    char* processed_html = NULL;

    if (should_process_templates) {
        printf("%s%s[TEMPLATE] %sProcessing template features in %s%s\n", 
               BOLD, COLOR_MAGENTA, COLOR_RESET, html_file, COLOR_RESET);
        
        char port_str[10];
        snprintf(port_str, sizeof(port_str), "%d", server_port);
        const char* keys[] = { 
            "user", "is_logged_in", "port", "enable_templates"
        };
        const char* values[] = { 
            "Dexter", "1", port_str, enable_templates ? "true" : "false"
        };
        
        printf("%s%s[TEMPLATE] %sStarting template processing with %d variables%s\n", 
               BOLD, COLOR_MAGENTA, COLOR_RESET, 4, COLOR_RESET);
        
        processed_html = process_template_auto(html_content, keys, values, 4);
        if (processed_html) {
            printf("%s%s[TEMPLATE] %sTemplate processing completed successfully%s\n", 
                   BOLD, COLOR_GREEN, COLOR_RESET, COLOR_RESET);
            free(html_content);
            html_content = NULL;
            
            if (is_db_initialized()) {
                printf("%s%s[SQLite] %sProcessing SQL queries in template%s\n", 
                       BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
                       
                char* sql_processed = process_sqlite_queries(processed_html);
                if (sql_processed != processed_html) {
                    free(processed_html);
                    processed_html = sql_processed;
                    printf("%s%s[SQLite] %sSQL query processing completed%s\n", 
                           BOLD, COLOR_GREEN, COLOR_RESET, COLOR_RESET);
                }
            }
        } else {
            printf("%s%s[TEMPLATE] %s%sTemplate processing failed, serving original content%s\n", 
                   BOLD, COLOR_RED, BOLD, COLOR_RESET, COLOR_RESET);
            processed_html = html_content;
            html_content = NULL;
            
            if (is_db_initialized()) {
                printf("%s%s[SQLite] %sProcessing SQL queries%s\n", 
                       BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
                       
                char* sql_processed = process_sqlite_queries(processed_html);
                if (sql_processed != processed_html) {
                    free(processed_html);
                    processed_html = sql_processed;
                    printf("%s%s[SQLite] %sSQL query processing completed%s\n", 
                           BOLD, COLOR_GREEN, COLOR_RESET, COLOR_RESET);
                }
            }
        }
    } else {
        if (!enable_templates) {
            printf("%s%s[TEMPLATE] %sTemplate processing is disabled globally%s\n", 
                  BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_RESET);
        } else {
            printf("%s%s[TEMPLATE] %sNo template features found, serving without processing%s\n", 
                  BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_RESET);
        }
        processed_html = html_content;
        html_content = NULL;
        
        if (is_db_initialized()) {
            printf("%s%s[SQLite] %sProcessing SQL queries%s\n", 
                   BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
                   
            char* sql_processed = process_sqlite_queries(processed_html);
            if (sql_processed != processed_html) {
                free(processed_html);
                processed_html = sql_processed;
                printf("%s%s[SQLite] %sSQL query processing completed%s\n", 
                       BOLD, COLOR_GREEN, COLOR_RESET, COLOR_RESET);
            }
        }
    }

    static char* stored_form_result = NULL;
    if (stored_form_result) {
        char* inject_point = strstr(processed_html, "</body>");
        if (inject_point) {
            size_t prefix_len = inject_point - processed_html;
            size_t suffix_len = strlen(inject_point);
            size_t result_len = strlen(stored_form_result);
            size_t new_len = prefix_len + result_len + suffix_len + 1;
            
            char* new_html = malloc(new_len);
            if (new_html) {
                memcpy(new_html, processed_html, prefix_len);
                memcpy(new_html + prefix_len, stored_form_result, result_len);
                memcpy(new_html + prefix_len + result_len, inject_point, suffix_len);
                new_html[new_len - 1] = '\0';
                
                free(processed_html);
                processed_html = new_html;
            }
        }
        
        free(stored_form_result);
        stored_form_result = NULL;
    }

    char* final_html = inject_hot_reload_js(processed_html);
    if (!final_html) {
        const char* error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n"
                                     "<h1>500 Internal Server Error</h1><p>Hot reload script injection failed</p>";
        write(new_socket, error_response, strlen(error_response));
        close(new_socket);
        return;
    }

    const char* headers = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/html; charset=UTF-8\r\n"
                         "Connection: keep-alive\r\n"
                         "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
                         "Pragma: no-cache\r\n"
                         "Access-Control-Allow-Origin: *\r\n"
                         "\r\n";
    
    write(new_socket, headers, strlen(headers));
    write(new_socket, final_html, strlen(final_html));
    free(final_html);
    final_html = NULL;
    close(new_socket);
}

void handle_websocket_client(int new_socket, ws_clients_t* clients) {
    if (!clients || new_socket <= 0) {
        if (new_socket > 0) close(new_socket);
        return;
    }

    char buffer[BUFFER_SIZE] = { 0 };
    
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if (setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        perror("setsockopt failed");
        close(new_socket);
        return;
    }
    
    ssize_t bytes_read = read(new_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        perror("Error reading WebSocket handshake request");
        close(new_socket);
        return;
    }
    
    buffer[bytes_read] = '\0';
    char* origin = strstr(buffer, "Origin: ");
    if (origin) {
        char* end = strstr(origin, "\r\n");
        if (end) {
            *end = '\0';
            printf("%s%s[WebSocket] %sConnection attempt from %s%s\n", 
                   BOLD, COLOR_BLUE, COLOR_RESET, origin + 8, COLOR_RESET);
            *end = '\r'; 
        }
    }
    
    if (is_websocket_request(buffer)) {
        printf("%s%s[WebSocket] %sHandshake received%s\n", 
               BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
        if (process_ws_handshake(new_socket, buffer) == 0) {
            printf("%s%s[WebSocket] %s%sHandshake successful%s\n", 
                   BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET);
            add_ws_client(clients, new_socket);
            return;
        } else {
            printf("%s%s[WebSocket] %s%sHandshake failed%s\n", 
                   BOLD, COLOR_BLUE, BOLD, COLOR_RED, COLOR_RESET);
        }
    } else {
        printf("%s%s[WebSocket] %s%sNot a valid WebSocket upgrade request%s\n", 
               BOLD, COLOR_BLUE, BOLD, COLOR_RED, COLOR_RESET);
    }
    
    close(new_socket);
}

void set_template_settings(bool enabled) {
    enable_templates = enabled;
}