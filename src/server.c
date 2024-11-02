#include "server.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Initialize server socket
    server_fd = initialize_server(&address);
    if (server_fd == -1) {
        return EXIT_FAILURE;
    }

    printf("Server listening on port: %d\n", PORT);

    while (1) { // Keep the server running
        // Accept a new connection
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Connection not accepted!");
            continue; // Continue to the next iteration to accept new connections
        }
        handle_client(new_socket); // Handle the client request
    }

    // Close the server socket (this line won't be reached in the current loop)
    close(server_fd);
    return 0;
}
