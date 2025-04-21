#include "handle_client.h"
#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define MAX_PATH_LEN 256
#define READ_TIMEOUT_SECS 5

static int set_socket_timeout(int sockfd, int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt - SO_RCVTIMEO");
        return -1;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt - SO_SNDTIMEO");
        return -1;
    }
    
    return 0;
}

const char* get_content_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".html") == 0) return "text/html";
        if (strcmp(ext, ".css") == 0) return "text/css";
        if (strcmp(ext, ".js") == 0) return "application/javascript";
        if (strcmp(ext, ".json") == 0) return "application/json";
        if (strcmp(ext, ".png") == 0) return "image/png";
        if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(ext, ".gif") == 0) return "image/gif";
        if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
        if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    }
    return "text/plain";
}

static int read_request_line(int client_socket, char* buffer, size_t buffer_size) {
    if (set_socket_timeout(client_socket, READ_TIMEOUT_SECS) < 0) {
        return -1;
    }  
    size_t total_read = 0;
    char c;
    ssize_t bytes_read;
    
    while (total_read < buffer_size - 1) {
        bytes_read = recv(client_socket, &c, 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                fprintf(stderr, "Timeout while reading request\n");
            } else if (bytes_read < 0) {
                perror("Error reading from socket");
            }
            return -1;
        } 
        buffer[total_read++] = c;
        if (total_read >= 2 && 
            buffer[total_read-2] == '\r' && 
            buffer[total_read-1] == '\n') {
            break;
        }
    }
    
    buffer[total_read] = '\0';
    return total_read;
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    char path[MAX_PATH_LEN] = {0};
    char full_path[MAX_PATH_LEN] = {0};
    int file_fd = -1;
    if (read_request_line(client_socket, buffer, BUFFER_SIZE) <= 0) {
        close(client_socket);
        return;
    }
    if (sscanf(buffer, "GET %s", path) != 1) {
        fprintf(stderr, "Invalid request format: %s\n", buffer);
        close(client_socket);
        return;
    }
    
    while (read_request_line(client_socket, buffer, BUFFER_SIZE) > 0) {
        if (strcmp(buffer, "\r\n") == 0) {
            break;
        }
    }
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }
    
    snprintf(full_path, MAX_PATH_LEN, "%s%s", HTML_DIR, path);
    struct stat file_stat;
    if (stat(full_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        file_fd = open(full_path, O_RDONLY);
    }
    
    if (file_fd == -1) {
        const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 26\r\nContent-Type: text/html\r\n\r\n<h1>404 - Not Found</h1>";
        send(client_socket, not_found, strlen(not_found), 0);
    } else {
        const char* content_type = get_content_type(path);
        char headers[BUFFER_SIZE];
        snprintf(headers, BUFFER_SIZE,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %ld\r\n"
                "Content-Type: %s\r\n"
                "Connection: close\r\n"
                "\r\n",
                file_stat.st_size, content_type);
        
        if (send(client_socket, headers, strlen(headers), 0) < 0) {
            perror("Error sending headers");
            close(file_fd);
            close(client_socket);
            return;
        }
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
            ssize_t bytes_sent = send(client_socket, buffer, bytes_read, 0);
            if (bytes_sent < 0) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    break;
                }
                perror("Error sending file content");
                break;
            }
        }
        close(file_fd);
    }
    close(client_socket);
}

int is_websocket_request(const char* request) {
    return (strstr(request, "Upgrade: websocket") != NULL) ? 1 : 0;
}

void handle_websocket_client(int client_socket, ws_clients_t* clients) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';
    
    if (process_ws_handshake(client_socket, buffer)) {
        add_ws_client(clients, client_socket);
        printf("%s%s[WebSocket] %s%sClient connected%s\n", 
               BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET);
    } else {
        fprintf(stderr, "%s%s[WebSocket] %s%sFailed to process WebSocket handshake%s\n", 
                BOLD, COLOR_RED, BOLD, COLOR_YELLOW, COLOR_RESET);
        close(client_socket);
    }
}

int process_ws_handshake(int client_socket, char* buffer) {
    return process_ws_handshake(client_socket, buffer);
} 