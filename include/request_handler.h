#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "html_serve.h"
#include "template.h"

#define BUFFER_SIZE 1024

void handle_client(int new_socket);

#endif // REQUEST_HANDLER_H
