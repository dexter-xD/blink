#include "request_handler.h"
#include "websocket.h"

bool enable_templates = true;
char* custom_html_file = NULL;
int server_port = PORT;
const char* default_html_file = "index.html";

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
    char buffer[BUFFER_SIZE] = { 0 };
    ssize_t bytes_read = read(new_socket, buffer, BUFFER_SIZE - 1);
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
    
    if (strstr(buffer, "GET /") != NULL && strstr(buffer, "GET /favicon.ico") == NULL) {
        printf("%s%s[HTTP] %sRequest: %.*s%s\n", 
               BOLD, COLOR_GREEN, COLOR_RESET, 50, buffer, COLOR_RESET);
    }
    
    if (strstr(buffer, "GET /favicon.ico") != NULL) {
        close(new_socket);
        return;
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
        char* request_path = extract_path(buffer);
        
        if (request_path) {
            if (strcmp(request_path, "/advanced") == 0) {
                strcpy(html_file, "advanced.html");
            } else if (strcmp(request_path, "/") != 0 && strncmp(request_path, "/index", 6) != 0) {
                char potential_file[256];
                snprintf(potential_file, sizeof(potential_file), "%.*s.html", (int)strcspn(request_path + 1, "?#"), request_path + 1);
                
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", HTML_DIR, potential_file);
                
                FILE* test_file = fopen(full_path, "r");
                if (test_file) {
                    fclose(test_file);
                    strcpy(html_file, potential_file);
                }
            }
        }
    }

    char path[512];
    if (custom_html_file) {
        if (custom_html_file[0] == '/') {
            strncpy(path, custom_html_file, sizeof(path) - 1);
        } 
        else {
            strncpy(path, custom_html_file, sizeof(path) - 1);
        }
        path[sizeof(path) - 1] = '\0';
    } else {
        snprintf(path, sizeof(path), "%s/%s", HTML_DIR, html_file);
    }
    
    printf("%s%s[HTTP] %sServing HTML file: %s%s%s\n", 
           BOLD, COLOR_GREEN, COLOR_RESET, COLOR_CYAN, path, COLOR_RESET);
    
    char* html_content = serve_html(path);
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
        } else {
            printf("%s%s[TEMPLATE] %s%sTemplate processing failed, serving original content%s\n", 
                   BOLD, COLOR_RED, BOLD, COLOR_RESET, COLOR_RESET);
            processed_html = html_content;
            html_content = NULL;
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
