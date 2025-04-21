#include "websocket.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>


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
        perror("Failed to allocate memory for WebSocket clients");
        return NULL;
    } 
    memset(clients->client_sockets, 0, sizeof(clients->client_sockets));
    clients->count = 0;
    if (pthread_mutex_init(&clients->mutex, NULL) != 0) {
        perror("Failed to initialize WebSocket clients mutex");
        free(clients);
        return NULL;
    }
    
    return clients;
}

static int make_socket_non_blocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get");
        return -1;
    }
    
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set");
        return -1;
    }
    
    return 0;
}

void add_ws_client(ws_clients_t* clients, int socket_fd) {
    if (!clients || socket_fd <= 0) return;
    if (make_socket_non_blocking(socket_fd) < 0) {
        fprintf(stderr, "Warning: Failed to make WebSocket socket non-blocking\n");
    }
    
    pthread_mutex_lock(&clients->mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->client_sockets[i] == socket_fd) {
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
                printf("WebSocket client added, socket: %d, total clients: %d\n", socket_fd, clients->count);
                added = 1;
                break;
            }
        }
    } 
    
    if (!added) {
        fprintf(stderr, "Maximum WebSocket clients reached (%d), cannot add more\n", MAX_CLIENTS);
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
            printf("WebSocket client removed, socket: %d, total clients: %d\n", socket_fd, clients->count);
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
    
    char* key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) {
        return -1;
    }
    
    key_start += 19;
    char* key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        return -1;
    }
    size_t key_length = key_end - key_start;
    if (key_length >= 256) {
        return -1;
    }
    
    char ws_key[256] = {0};
    strncpy(ws_key, key_start, key_length);
    char combined_key[512] = {0};
    sprintf(combined_key, "%s%s", ws_key, WS_HANDSHAKE_KEY);
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)combined_key, strlen(combined_key), hash);
    char* encoded_hash = base64_encode(hash, SHA_DIGEST_LENGTH);
    if (!encoded_hash) {
        return -1;
    }
    char response[512];
    snprintf(response, sizeof(response), WS_RESPONSE, encoded_hash);
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);   
    ssize_t bytes_sent = send(client_socket, response, strlen(response), 0); 
    free(encoded_hash);
    return (bytes_sent > 0) ? 0 : -1;
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
                printf("WebSocket client %d disconnected, removing\n", socket_to_close);
                close(socket_to_close);
            }
        }
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients->client_sockets[i] != 0) {
            int result = send_ws_frame(clients->client_sockets[i], message, strlen(message), WS_TEXT);
            if (result < 0) {
                if (!is_ping) {
                    printf("Failed to send to client %d, removing\n", clients->client_sockets[i]);
                }
                
                int socket_to_close = clients->client_sockets[i];
                clients->client_sockets[i] = 0;
                clients->count--;
                close(socket_to_close);
            }
        }
    }
    
    pthread_mutex_unlock(&clients->mutex);
}

void handle_ws_connection(int client_socket, ws_clients_t* clients) {
    if (!clients || client_socket <= 0) {
        if (client_socket > 0) close(client_socket);
        return;
    }
    

    memset(ws_recv_buffer, 0, BUFFER_SIZE);
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    ssize_t bytes_read = read(client_socket, ws_recv_buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        close(client_socket);
        return;
    }
    
    ws_recv_buffer[bytes_read] = '\0';
    if (strstr(ws_recv_buffer, "Upgrade: websocket") && strstr(ws_recv_buffer, "Connection: Upgrade")) {
        if (process_ws_handshake(client_socket, ws_recv_buffer) == 0) {
            add_ws_client(clients, client_socket);
            return;
        }
    }
    close(client_socket);
}

void free_ws_clients(ws_clients_t* clients) {
    if (clients) {
        pthread_mutex_lock(&clients->mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients->client_sockets[i] != 0) {
                printf("Closing WebSocket client %d during cleanup\n", clients->client_sockets[i]);me
                send_ws_frame(clients->client_sockets[i], "", 0, WS_CLOSE);
                close(clients->client_sockets[i]);
                clients->client_sockets[i] = 0;
            }
        }
        clients->count = 0;
        pthread_mutex_unlock(&clients->mutex);
        pthread_mutex_destroy(&clients->mutex);
        free(clients);
    }
} 