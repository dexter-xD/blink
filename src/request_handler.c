#include "request_handler.h"

int is_websocket_request(const char* buffer) {
    return (strstr(buffer, "Upgrade: websocket") && strstr(buffer, "Connection: Upgrade"));
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
    if (strstr(buffer, "GET /") != NULL && strstr(buffer, "GET /favicon.ico") == NULL) {
        printf("Connection accepted\n");
        printf("Request: %s\n", buffer);
    }
    if (strstr(buffer, "GET /favicon.ico") != NULL) {
        close(new_socket);
        return;
    }

    char* html_content = serve_html("../html/index.html");
    if (!html_content) {
        const char* not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
        write(new_socket, not_found_response, strlen(not_found_response));
        close(new_socket);
        return;
    }

    const char* keys[] = { "user", "is_logged_in" };
    const char* values[] = { "Dexter", "1" };
    const char* loop_key = "item";
    const char* loop_values[] = { "Item 1", "Item 2", "Item 3", "Item 4" };
    char* processed_html = process_template(html_content, keys, values, 2, loop_key, loop_values, 4);
    free(html_content);
    html_content = NULL;
    
    if (!processed_html) {
        const char* error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<h1>500 Internal Server Error</h1><p>Template processing failed</p>";
        write(new_socket, error_response, strlen(error_response));
        close(new_socket);
        return;
    }

    char* final_html = inject_hot_reload_js(processed_html);
    if (!final_html) {
        const char* error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html\r\n\r\n<h1>500 Internal Server Error</h1><p>Hot reload script injection failed</p>";
        write(new_socket, error_response, strlen(error_response));
        close(new_socket);
        return;
    }

    write(new_socket, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n", 48);
    write(new_socket, final_html, strlen(final_html));
    free(final_html);
    final_html = NULL;
    close(new_socket);
}

void handle_websocket_client(int new_socket, ws_clients_t* clients) {
    char buffer[BUFFER_SIZE] = { 0 };
    ssize_t bytes_read = read(new_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        close(new_socket);
        return;
    }
    

    buffer[bytes_read] = '\0';
    if (is_websocket_request(buffer)) {
        if (process_ws_handshake(new_socket, buffer) == 0) {
            add_ws_client(clients, new_socket);
            return;
        }
    }
    close(new_socket);
}
