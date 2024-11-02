#include "socket_utils.h"


// Function to initialize a server socket and bind it to the specified address
int initialize_server(struct sockaddr_in* address) {
    int server_fd;
    int opt = 1;

    // Create a socket with IPv4 and TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed!");  // Error if socket creation fails
        return -1;
    }

    // Set socket options to reuse address and port, allowing quick rebinds after crashes
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) != 0) {
        perror("setsockopt failed");  // Error if setting options fails
        close(server_fd);  // Close socket if options cannot be set
        return -1;
    }

    // Define the server address and port
    address->sin_family = AF_INET;         // IPv4
    address->sin_addr.s_addr = INADDR_ANY; // Listen on all available network interfaces
    address->sin_port = htons(PORT);       // Set port to listen on (convert to network byte order)

    // Bind the socket to the specified address and port
    if (bind(server_fd, (struct sockaddr*)address, sizeof(*address)) < 0) {
        perror("Bind failed!");  // Error if binding fails
        close(server_fd);  // Close socket if binding fails
        return -1;
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed!");  // Error if listening fails
        close(server_fd);  // Close socket if listen fails
        return -1;
    }

    return server_fd;  // Return the file descriptor for the listening socket
}

// Function to read data from a client and print it to the console
void read_client_data(int socket, char* buffer) {
    ssize_t read_value;

    // Loop to continuously read data from the client
    while ((read_value = read(socket, buffer, BUFFER_SIZE)) > 0) {
        printf("Client: %s", buffer);  // Print client data to the console
        memset(buffer, 0, BUFFER_SIZE); // Clear buffer for the next read
    }
}
