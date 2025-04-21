#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_CLIENTS 50
#define BUFFER_SIZE 1024
#define WS_HANDSHAKE_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_RESPONSE "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"

#define WS_TEXT 0x01
#define WS_BINARY 0x02
#define WS_CLOSE 0x08
#define WS_PING 0x09
#define WS_PONG 0x0A

typedef struct {
    int client_sockets[MAX_CLIENTS];
    int count;
    pthread_mutex_t mutex;
} ws_clients_t;

ws_clients_t* init_ws_clients();
void add_ws_client(ws_clients_t* clients, int socket_fd);
void remove_ws_client(ws_clients_t* clients, int socket_fd);
bool is_client_connected(int socket_fd);
int process_ws_handshake(int client_socket, char* buffer);
int send_ws_frame(int client_socket, const char* message, size_t length, int opcode);
void broadcast_to_ws_clients(ws_clients_t* clients, const char* message);
void handle_ws_connection(int client_socket, ws_clients_t* clients);
void free_ws_clients(ws_clients_t* clients);

#endif