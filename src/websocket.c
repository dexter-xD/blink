#include "websocket.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <endian.h>
#include "request_handler.h" 

// Function declarations
static char* base64_encode(const unsigned char* input, int length);
static int make_socket_non_blocking(int socket_fd);
static void cleanup_ws_client(ws_clients_t* clients, int client_socket);
static char* create_ws_frame(const char* message, size_t length, int opcode, size_t* frame_size);
static void dump_client_list(ws_clients_t* clients);
int is_websocket_request(const char* buffer); 

#ifndef ntohll
#if __BYTE_ORDER == __LITTLE_ENDIAN
uint64_t ntohll(uint64_t x) {
    return ((uint64_t)ntohl((uint32_t)x) << 32) | ntohl((uint32_t)(x >> 32));
}
#else
uint64_t ntohll(uint64_t x) {
    return x;
}
#endif
#endif

char ws_recv_buffer[BUFFER_SIZE];

static char* base64_encode(const unsigned char* input, int length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    char* base64_encoded = malloc(bufferPtr->length + 1);
    if (!base64_encoded) {
        BIO_free_all(bio);
        return NULL;
    }
    
    memcpy(base64_encoded, bufferPtr->data, bufferPtr->length);
    base64_encoded[bufferPtr->length] = 0;
    
    BIO_free_all(bio);
    
    return base64_encoded;
}

ws_clients_t* init_ws_clients() {
    ws_clients_t* clients = malloc(sizeof(ws_clients_t));
    if (!clients) {
        fprintf(stderr, "%s%sERROR: %sFailed to allocate memory for WebSocket clients%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        return NULL;
    } 
    
    memset(clients->client_sockets, 0, sizeof(clients->client_sockets));
    clients->count = 0;
    
    if (pthread_mutex_init(&clients->mutex, NULL) != 0) {
        fprintf(stderr, "%s%sERROR: %sFailed to initialize WebSocket clients mutex%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        free(clients);
        return NULL;
    }
    
    printf("%s%s[WebSocket] %sClient manager initialized (max clients: %d)%s\n", 
           BOLD, COLOR_BLUE, COLOR_GREEN, MAX_CLIENTS, COLOR_RESET);
    return clients;
}

static int make_socket_non_blocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        fprintf(stderr, "%s%sERROR: %sfcntl get: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        return -1;
    }
    
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "%s%sERROR: %sfcntl set: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        return -1;
    }
    
    return 0;
}

void add_ws_client(ws_clients_t* clients, int socket_fd) {
    if (!clients || socket_fd <= 0) {
        printf("%s%s[WebSocket] %sInvalid arguments to add_ws_client%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        return;
    }
    
    if (make_socket_non_blocking(socket_fd) < 0) {
        fprintf(stderr, "%s%s[WARNING] %sFailed to make WebSocket socket non-blocking%s\n", 
                BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_RESET);
    }
    
    char client_info[64] = "unknown";
    char client_ip[INET_ADDRSTRLEN] = {0};
    int client_port = 0;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(socket_fd, (struct sockaddr *)&addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &(addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        client_port = ntohs(addr.sin_port);
        snprintf(client_info, sizeof(client_info), "%s:%d", client_ip, client_port);
    }
    
    pthread_mutex_lock(&clients->mutex);
    
    if (client_ip[0] != '\0') {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients->client_sockets[i] != 0 && clients->client_sockets[i] != socket_fd) {
                struct sockaddr_in existing_addr;
                socklen_t existing_len = sizeof(existing_addr);
                char existing_ip[INET_ADDRSTRLEN] = {0};
                
                if (getpeername(clients->client_sockets[i], (struct sockaddr *)&existing_addr, &existing_len) == 0) {
                    inet_ntop(AF_INET, &(existing_addr.sin_addr), existing_ip, INET_ADDRSTRLEN);
                    if (strcmp(client_ip, existing_ip) == 0) {
                        int old_socket = clients->client_sockets[i];
                        printf("%s%s[WebSocket] %sReplacing existing connection from %s (socket %d → %d)%s\n", 
                               BOLD, COLOR_BLUE, COLOR_YELLOW, existing_ip, old_socket, socket_fd, COLOR_RESET);
                        
                        close(old_socket);
                        clients->client_sockets[i] = socket_fd;
                        pthread_mutex_unlock(&clients->mutex);
                        printf("%s%s[WebSocket] %sClient reconnected, total clients: %s%d%s\n", 
                               BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_YELLOW, clients->count, COLOR_RESET);
                        dump_client_list(clients);
                        return;
                    }
                }
            }
        }
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->client_sockets[i] == socket_fd) {
            printf("%s%s[WebSocket] %sClient already exists, socket: %d (%s)%s\n", 
                   BOLD, COLOR_BLUE, COLOR_YELLOW, socket_fd, client_info, COLOR_RESET);
            pthread_mutex_unlock(&clients->mutex);
            return;
        }
    }
    
    int added = 0;
    if (clients->count < MAX_CLIENTS) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients->client_sockets[i] == 0) {
                clients->client_sockets[i] = socket_fd;
                clients->count++;
                added = 1;
                printf("%s%s[WebSocket] %sClient added, socket: %d (%s%s%s), total clients: %s%d%s\n", 
                       BOLD, COLOR_BLUE, COLOR_GREEN, socket_fd, COLOR_CYAN, client_info, COLOR_GREEN, 
                       COLOR_YELLOW, clients->count, COLOR_RESET);
                break;
            }
        }
    }
    
    if (!added) {
        fprintf(stderr, "%s%s[WebSocket] %sMaximum clients reached (%d), cannot add more%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, MAX_CLIENTS, COLOR_RESET);
        close(socket_fd);
    }
    
    pthread_mutex_unlock(&clients->mutex);
    dump_client_list(clients);
}

static void dump_client_list(ws_clients_t* clients) {
    if (!clients) return;
    
    pthread_mutex_lock(&clients->mutex);
    
    printf("%s%s[WebSocket] %sCurrent clients (%d total):%s\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, clients->count, COLOR_RESET);
    
    int active_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->client_sockets[i] != 0) {
            char client_desc[64] = "unknown";
            struct sockaddr_in caddr;
            socklen_t caddr_len = sizeof(caddr);
            if (getpeername(clients->client_sockets[i], (struct sockaddr *)&caddr, &caddr_len) == 0) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(caddr.sin_addr), ip, INET_ADDRSTRLEN);
                snprintf(client_desc, sizeof(client_desc), "%s:%d", ip, ntohs(caddr.sin_port));
            }
            printf("  %s[%d]%s Socket %s%d%s: %s%s%s\n", 
                   COLOR_YELLOW, i, COLOR_RESET, COLOR_CYAN, clients->client_sockets[i], 
                   COLOR_RESET, COLOR_GREEN, client_desc, COLOR_RESET);
            active_count++;
        }
    }
    
    if (active_count == 0) {
        printf("  %sNo active clients%s\n", COLOR_YELLOW, COLOR_RESET);
    }
    
    pthread_mutex_unlock(&clients->mutex);
}

void remove_ws_client(ws_clients_t* clients, int socket_fd) {
    if (!clients || socket_fd <= 0) return;
    
    pthread_mutex_lock(&clients->mutex);
    
    int removed = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->client_sockets[i] == socket_fd) {
            clients->client_sockets[i] = 0;
            clients->count--;
            removed = 1;
            printf("%s%s[WebSocket] %sClient removed, socket: %s%d%s, total clients: %s%d%s\n", 
                   BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, socket_fd, COLOR_YELLOW, 
                   COLOR_GREEN, clients->count, COLOR_RESET);
            break;
        }
    }
    
    pthread_mutex_unlock(&clients->mutex);
    if (removed) {
        close(socket_fd);
    }
}

bool is_client_connected(int socket_fd) {
    if (socket_fd <= 0) return false;
    fd_set read_fds;
    struct timeval tv = {0, 0};
    
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);
    
    int result = select(socket_fd + 1, &read_fds, NULL, NULL, &tv);
    
    if (result < 0) {
        return false;
    } else if (result > 0) {
        char buffer[1];
        result = recv(socket_fd, buffer, 1, MSG_PEEK | MSG_DONTWAIT);       
        if (result == 0) {
            return false;
        } else if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return true;
            }
            return false;
        }
        return true;
    }   
    return true; 
}

int process_ws_handshake(int client_socket, char* buffer) {
    if (client_socket <= 0 || !buffer) return -1;
    
    printf("%s%s[WebSocket] %sProcessing handshake for client %s%d%s\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, COLOR_CYAN, client_socket, COLOR_RESET);

    if (!strstr(buffer, "Upgrade: websocket")) {
        printf("%s%s[WebSocket] %sHandshake failed: %s'Upgrade: websocket' header missing%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_YELLOW, COLOR_RESET);
        return -1;
    }
    

    if (!strstr(buffer, "Connection: Upgrade")) {
        printf("%s%s[WebSocket] %sHandshake failed: %s'Connection: Upgrade' header missing%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_YELLOW, COLOR_RESET);
        return -1;
    }
    

    char* key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) {
        printf("%s%s[WebSocket] %sHandshake failed: %s'Sec-WebSocket-Key' header missing%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_YELLOW, COLOR_RESET);
        return -1;
    }
    
    key_start += 19;
    char* key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        printf("%s%s[WebSocket] %sHandshake failed: %sInvalid 'Sec-WebSocket-Key' format%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_YELLOW, COLOR_RESET);
        return -1;
    }
    
    size_t key_length = key_end - key_start;
    if (key_length >= 256) {
        printf("%s%s[WebSocket] %sHandshake failed: %s'Sec-WebSocket-Key' too long%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_YELLOW, COLOR_RESET);
        return -1;
    }
    
    char ws_key[256] = {0};
    strncpy(ws_key, key_start, key_length);
    printf("%s%s[WebSocket] %sKey: %s%s%s\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, COLOR_CYAN, ws_key, COLOR_RESET);
    
    char combined_key[512] = {0};
    sprintf(combined_key, "%s%s", ws_key, WS_HANDSHAKE_KEY);
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)combined_key, strlen(combined_key), hash);
    
    char* encoded_hash = base64_encode(hash, SHA_DIGEST_LENGTH);
    if (!encoded_hash) {
        printf("%s%s[WebSocket] %sHandshake failed: %sBase64 encoding failed%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_YELLOW, COLOR_RESET);
        return -1;
    }
    
    printf("%s%s[WebSocket] %sAccept key: %s%s%s\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, COLOR_MAGENTA, encoded_hash, COLOR_RESET);
    
    // Construct the response
    char response[512];
    snprintf(response, sizeof(response), 
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n"
             "\r\n", encoded_hash);
    
    struct timeval tv;
    tv.tv_sec = 5; 
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    ssize_t bytes_sent = send(client_socket, response, strlen(response), 0);  
    free(encoded_hash);  
    if (bytes_sent <= 0) {
        printf("%s%s[WebSocket] %sHandshake failed: %sFailed to send response (%s)%s\n", 
               BOLD, COLOR_RED, COLOR_RESET, COLOR_YELLOW, strerror(errno), COLOR_RESET);
        return -1;
    }
    
    printf("%s%s[WebSocket] %s%sHandshake successful%s, sent %s%zd%s bytes\n", 
           BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET, COLOR_YELLOW, bytes_sent, COLOR_RESET);
    return 0;
}

static char* create_ws_frame(const char* message, size_t length, int opcode, size_t* frame_size) {
    if (!message || !frame_size) return NULL;
    size_t header_size = 2; 
    if (length > 125 && length <= 65535) {
        header_size += 2; 
    } else if (length > 65535) {
        header_size += 8;
    }
    char* frame = malloc(header_size + length);
    if (!frame) {
        perror("Memory allocation failed for WebSocket frame");
        return NULL;
    }
    
    frame[0] = 0x80 | (opcode & 0x0F); 
    if (length <= 125) {
        frame[1] = (char)length; 
    } else if (length <= 65535) {
        frame[1] = 126; 
        frame[2] = (length >> 8) & 0xFF; 
        frame[3] = length & 0xFF; 
    } else {
        frame[1] = 127; 
        for (int i = 0; i < 8; i++) {
            frame[2 + i] = (length >> ((7 - i) * 8)) & 0xFF;
        }
    }
    
    memcpy(frame + header_size, message, length);   
    *frame_size = header_size + length;
    return frame;
}

int send_ws_frame(int client_socket, const char* message, size_t length, int opcode) {
    if (client_socket <= 0 || !message) return -1;
    
    if (!is_client_connected(client_socket)) {
        return -1;
    }
    
    size_t frame_size;
    char* frame = create_ws_frame(message, length, opcode, &frame_size);
    if (!frame) {
        return -1;
    }
    
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    size_t total_sent = 0;
    while (total_sent < frame_size) {
        ssize_t bytes_sent = send(client_socket, frame + total_sent, frame_size - total_sent, 0);
        
        if (bytes_sent <= 0) {
            if (errno != EPIPE && errno != ECONNRESET && 
                errno != EAGAIN && errno != EWOULDBLOCK && errno != ETIMEDOUT) {
                printf("Socket %d send error: %s\n", client_socket, strerror(errno));
            }
            free(frame);
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    free(frame);
    return 0;
}

static void cleanup_ws_client(ws_clients_t* clients, int client_socket) {
    if (!clients || client_socket <= 0) return;
    remove_ws_client(clients, client_socket);
    char close_frame[2] = {0x88, 0x00};
    struct timeval tv = {0, 100000};
    setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    send(client_socket, close_frame, 2, 0);
    close(client_socket);
}

void broadcast_to_ws_clients(ws_clients_t* clients, const char* message) {
    if (!clients || !message) return;
    bool is_ping = (strcmp(message, "ping") == 0);
    
    pthread_mutex_lock(&clients->mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->client_sockets[i] != 0) {
            if (!is_client_connected(clients->client_sockets[i])) {
                int socket_to_close = clients->client_sockets[i];
                clients->client_sockets[i] = 0;
                clients->count--;
                printf("%s%s[WebSocket] %sClient %s%d%s disconnected, removing\n", 
                       BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, socket_to_close, COLOR_RESET);
                close(socket_to_close);
            }
        }
    }
    
    int send_success = 0;
    int send_failures = 0;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->client_sockets[i] != 0) {
            int result = send_ws_frame(clients->client_sockets[i], message, strlen(message), WS_TEXT);
            if (result < 0) {
                if (!is_ping) {
                    printf("%s%s[WebSocket] %sFailed to send to client %s%d%s, removing\n", 
                           BOLD, COLOR_RED, COLOR_RESET, COLOR_CYAN, clients->client_sockets[i], COLOR_RESET);
                }
                
                int socket_to_close = clients->client_sockets[i];
                clients->client_sockets[i] = 0;
                clients->count--;
                close(socket_to_close);
                send_failures++;
            } else {
                send_success++;
            }
        }
    }
    
    if (!is_ping && (send_success > 0 || send_failures > 0)) {
        printf("%s%s[WebSocket] %sBroadcast '%s%s%s': %s%d%s successful, %s%d%s failed, %s%d%s total clients\n", 
               BOLD, COLOR_BLUE, COLOR_RESET, COLOR_GREEN, message, COLOR_RESET, 
               COLOR_GREEN, send_success, COLOR_RESET, 
               COLOR_RED, send_failures, COLOR_RESET, 
               COLOR_YELLOW, clients->count, COLOR_RESET);
    }
    
    pthread_mutex_unlock(&clients->mutex);
}

void handle_ws_connection(int client_socket, ws_clients_t* clients) {
    if (client_socket <= 0 || !clients) {
        if (client_socket > 0) close(client_socket);
        return;
    }
    
    struct timeval tv;
    tv.tv_sec = 5; 
    tv.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        perror("setsockopt failed on WebSocket client socket");
        close(client_socket);
        return;
    }
    
    int yes = 1;
    if (setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) < 0) {
        perror("Failed to set SO_KEEPALIVE on WebSocket socket");
    }
    
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read <= 0) {
        perror("Error reading from WebSocket client");
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';
    char client_info[50] = "unknown";
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    if (getpeername(client_socket, (struct sockaddr *)&addr, &addr_len) == 0) {
        snprintf(client_info, sizeof(client_info), "%s:%d", 
                inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        printf("%s%s[WebSocket] %sConnection attempt from %s%s%s\n", 
               BOLD, COLOR_BLUE, COLOR_RESET, COLOR_CYAN, client_info, COLOR_RESET);
    }
    
    if (is_websocket_request(buffer)) {
        printf("%s%s[WebSocket] %s%sValid handshake%s received from %s%s%s\n", 
               BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET, 
               COLOR_CYAN, client_info, COLOR_RESET);
        
        if (process_ws_handshake(client_socket, buffer) == 0) {
            printf("%s%s[WebSocket] %s%sHandshake successful%s with %s%s%s, adding to clients\n", 
                   BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET, 
                   COLOR_CYAN, client_info, COLOR_RESET);
            add_ws_client(clients, client_socket);
            printf("%s%s[WebSocket] %s%sHandshake successful%s\n", 
                   BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET);

            if (send_ws_frame(client_socket, "ping", 4, WS_TEXT) == 0) {
                printf("%s%s[WebSocket] %sInitial ping sent to %s%s%s\n", 
                       BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, client_info, COLOR_RESET);
            } else {
                printf("%s%s[WebSocket] %s%sFailed to send initial ping%s to %s%s%s\n", 
                       BOLD, COLOR_BLUE, BOLD, COLOR_RED, COLOR_RESET, 
                       COLOR_CYAN, client_info, COLOR_RESET);
            }
            return;
        } else {
            printf("%s%s[WebSocket] %s%sHandshake failed%s with %s%s%s\n", 
                   BOLD, COLOR_BLUE, BOLD, COLOR_RED, COLOR_RESET, 
                   COLOR_CYAN, client_info, COLOR_RESET);
        }
    } else {
        printf("%s%s[WebSocket] %s%sInvalid request%s from %s%s%s\n", 
               BOLD, COLOR_RED, BOLD, COLOR_RED, COLOR_RESET, 
               COLOR_CYAN, client_info, COLOR_RESET);
        
        printf("%s%s[WebSocket] %sHeaders from %s%s%s:\n%s%s%s\n", 
               BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, client_info, COLOR_YELLOW, 
               COLOR_MAGENTA, buffer, COLOR_RESET);
    }
    
    close(client_socket);
}


int process_ws_frame(int client_socket, ws_clients_t* clients) {
    if (client_socket <= 0) return -1;
    
    unsigned char header[2];
    ssize_t bytes_read = recv(client_socket, header, 2, 0);
    if (bytes_read < 2) {
        return -1;
    }
    
    bool fin = (header[0] & 0x80) != 0;
    uint8_t opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;
    uint8_t mask_key[4] = {0};
    if (payload_len == 126) {
        uint16_t len16;
        bytes_read = recv(client_socket, &len16, 2, 0);
        if (bytes_read < 2) return -1;
        payload_len = ntohs(len16);
    } else if (payload_len == 127) {
        uint64_t len64;
        bytes_read = recv(client_socket, &len64, 8, 0);
        if (bytes_read < 8) return -1;
        payload_len = ntohll(len64);
    }
    
    if (masked) {
        bytes_read = recv(client_socket, mask_key, 4, 0);
        if (bytes_read < 4) return -1;
    }
    if (payload_len > BUFFER_SIZE - 1) {
        payload_len = BUFFER_SIZE - 1;
    }
    unsigned char* payload = malloc(payload_len + 1);
    if (!payload) return -1;   
    size_t bytes_to_read = payload_len;
    size_t total_read = 0;
    
    while (total_read < bytes_to_read) {
        bytes_read = recv(client_socket, payload + total_read, bytes_to_read - total_read, 0);
        if (bytes_read <= 0) {
            free(payload);
            return -1;
        }
        total_read += bytes_read;
    }
    
    if (masked) {
        for (size_t i = 0; i < payload_len; i++) {
            payload[i] ^= mask_key[i % 4];
        }
    }
    payload[payload_len] = '\0';
    switch (opcode) {
        case WS_TEXT:
            if (strcmp((char*)payload, "ping") != 0 && strcmp((char*)payload, "pong") != 0) {
                printf("%s%s[WebSocket] %sReceived text message: %s%s%s\n", 
                       BOLD, COLOR_BLUE, COLOR_RESET, COLOR_GREEN, payload, COLOR_RESET);
            }
            if (strcmp((char*)payload, "ping") == 0) {
                send_ws_frame(client_socket, "pong", 4, WS_TEXT);
            } else if (strcmp((char*)payload, "pong") == 0) {
                printf("%s%s[WebSocket] %sReceived pong from client %s%d%s\n", 
                       BOLD, COLOR_BLUE, COLOR_CYAN, COLOR_YELLOW, client_socket, COLOR_RESET);
            }
            break;
            
        case WS_BINARY:
            printf("%s%s[WebSocket] %sReceived binary message (%s%zu%s bytes)\n", 
                   BOLD, COLOR_BLUE, COLOR_RESET, COLOR_YELLOW, payload_len, COLOR_RESET);
            break;
            
        case WS_CLOSE:
            printf("%s%s[WebSocket] %sReceived close frame from client %s%d%s\n", 
                   BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, client_socket, COLOR_RESET);
            send_ws_frame(client_socket, "", 0, WS_CLOSE);
            remove_ws_client(clients, client_socket);
            break;
            
        case WS_PING:
            printf("%s%s[WebSocket] %sReceived ping from client %s%d%s\n", 
                   BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, client_socket, COLOR_RESET);
            send_ws_frame(client_socket, (char*)payload, payload_len, WS_PONG);
            break;
            
        case WS_PONG:
            printf("%s%s[WebSocket] %sReceived pong from client %s%d%s\n", 
                   BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, client_socket, COLOR_RESET);
            break;
            
        default:
            printf("%s%s[WebSocket] %sReceived unknown opcode %s%d%s from client %s%d%s\n", 
                   BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_RED, opcode, COLOR_YELLOW, 
                   COLOR_CYAN, client_socket, COLOR_RESET);
            break;
    }
    
    free(payload);
    return 0;
}

void free_ws_clients(ws_clients_t* clients) {
    if (clients) {
        pthread_mutex_lock(&clients->mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients->client_sockets[i] != 0) {
                printf("%s%s[WebSocket] %s%sClosing client %s%d%s during cleanup\n", 
                       BOLD, COLOR_BLUE, BOLD, COLOR_YELLOW, 
                       COLOR_CYAN, clients->client_sockets[i], COLOR_RESET);
                send_ws_frame(clients->client_sockets[i], "", 0, WS_CLOSE);
                close(clients->client_sockets[i]);
                clients->client_sockets[i] = 0;
            }
        }
        clients->count = 0;
        pthread_mutex_unlock(&clients->mutex);
        pthread_mutex_destroy(&clients->mutex);
        free(clients);
        
        printf("%s%s[WebSocket] %s%sClient manager cleaned up%s\n", 
               BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET);
    }
} 