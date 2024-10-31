// src/server.c
#include "../include/server.h"
#include "../include/socket_utils.h"

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = { 0 };

    // Initialize the server
    server_fd = initialize_server(&address);
    if (server_fd == -1) {
        return EXIT_FAILURE;
    }

    printf("Server listening on port: %d\n", PORT);

    // Accept a new connection
    if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Connection not accepted!");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("Connection accepted\n");

    // Read and print client data
    read_client_data(new_socket, buffer);

    close(server_fd);
    return 0;
}
