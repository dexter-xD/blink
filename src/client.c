#include <stdio.h>      // For standard input and output functions
#include <stdlib.h>     // For standard library functions, like exit
#include <string.h>     // For string manipulation functions, like strlen
#include <unistd.h>     // For close function to close the socket
#include <arpa/inet.h>  // For socket-related functions and data structures (specifically for IPv4 addresses)

int main() {
    // Step 1: Create a socket
    // The socket function creates an endpoint for communication and returns a socket descriptor.
    // AF_INET specifies the IPv4 address family.
    // SOCK_STREAM specifies a TCP socket (reliable, connection-oriented).
    // 0 means we’re using the default protocol for SOCK_STREAM, which is TCP.
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    // Step 2: Define the server address
    // `sockaddr_in` is a struct that holds information about the address to connect to.
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;           // Set the address family to IPv4
    server_addr.sin_port = htons(8080);         // Convert port 8080 to network byte order using htons (host to network short)
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Convert the local loopback IP "127.0.0.1" to a binary form
    
    // Step 3: Connect to the server
    // The connect function attempts to establish a connection to the specified server address.
    // It takes the socket descriptor, a pointer to the server address, and the size of the address structure.
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");  // Print an error message if the connection fails
        return 1;                     // Return with an error code
    }

    // Step 4: Send data to the server
    // The send function sends data to the server over the connected socket.
    // It takes the socket descriptor, the message, the message length, and flags (0 for no special options).
    char *message = "Hello, Server!";
    send(sock, message, strlen(message), 0);

    // Step 5: Close the socket
    // After the message is sent, close the socket to free up resources and end the connection.
    close(sock);
    
    return 0;  // Exit the program successfully
}
