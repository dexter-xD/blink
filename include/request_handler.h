#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "html_serve.h"
#include "template.h"
#include "websocket.h"
#include "server.h"
#include "sqlite_handler.h"

#define BUFFER_SIZE 1024

extern ws_clients_t* ws_clients;
extern bool enable_templates;

void handle_client(int new_socket);
void handle_websocket_client(int new_socket, ws_clients_t* clients);
int is_websocket_request(const char* buffer);
bool has_template_features(const char* content);
void set_template_settings(bool enabled);
void set_custom_html_file(const char* file_path);
void set_server_port(int port);

#endif 
