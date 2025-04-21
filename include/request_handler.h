#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "html_serve.h"
#include "template.h"
#include "websocket.h"

#define BUFFER_SIZE 1024

void handle_client(int new_socket);
void handle_websocket_client(int new_socket, ws_clients_t* clients);
int is_websocket_request(const char* buffer);

#endif 
