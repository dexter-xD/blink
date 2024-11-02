#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <asm-generic/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int initialize_server(struct sockaddr_in* address);
void read_client_data(int socket, char* buffer);

#endif
