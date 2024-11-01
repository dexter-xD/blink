// src/server.c
#include "../include/server.h"
#include "../include/socket_utils.h"
#include <pthread.h> // Include pthread for handling multiple clients
#include <string.h>

#define BUFFER_SIZE 1024 // Buffer size for reading client data

// Function prototype for handling client connections
void *handle_client(void *socket_desc);
void serve_html(int client_socket);

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Initialize the server
    server_fd = initialize_server(&address);
    if (server_fd == -1) {
        return EXIT_FAILURE;
    }

    printf("Server listening on port: %d\n", PORT);

    while (1) {
        // Accept a new connection
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Connection not accepted!");
            close(server_fd);
            return EXIT_FAILURE;
        }

        printf("Connection accepted\n");

        // Create a new thread to handle the client
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)&new_socket) < 0) {
            perror("Could not create thread");
            close(new_socket);
            continue; // Move to the next client
        }
        pthread_detach(client_thread); // Detach the thread to free resources when done
    }

    close(server_fd);
    return 0;
}

// Function to handle client connections
void *handle_client(void *socket_desc) {
    int client_socket = *(int *)socket_desc; // Get the client socket
    char buffer[BUFFER_SIZE] = {0}; // Buffer for reading client data

    // Read request from client
    read(client_socket, buffer, BUFFER_SIZE);

    // Check if the request is for the HTML page
    if (strstr(buffer, "GET / HTTP/1.1") != NULL) {
        serve_html(client_socket); // Serve the HTML page
    } else {
        // Handle TCP message (echoing for now)
        char *message = "Hello from TCP Server!";
        send(client_socket, message, strlen(message), 0);
    }

    close(client_socket); // Close the client socket
    return NULL; // Exit thread
}

// Function to serve HTML content
void serve_html(int client_socket) {
    // HTTP response for the HTML page
    const char *html_response = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html\r\n"
                                "Connection: close\r\n\r\n"
                                "<!DOCTYPE html>"
                                "<html lang='en'>"
                                "<head><meta charset='UTF-8'><title>TCP Client</title></head>"
                                "<body>"
                                "<h1>Send Message to TCP Server</h1>"
                                "<form id='messageForm'>"
                                "<label for='message'>Message:</label>"
                                "<input type='text' id='message' name='message' required>"
                                "<button type='submit'>Send</button>"
                                "</form>"
                                "<script>"
                                "document.getElementById('messageForm').addEventListener('submit', function (event) {"
                                "event.preventDefault();"
                                "const message = document.getElementById('message').value;"
                                "fetch('/send', {"
                                "method: 'POST',"
                                "headers: {'Content-Type': 'application/json'},"
                                "body: JSON.stringify({ message: message }),"
                                "}).then(response => response.json()).then(data => {"
                                "console.log('Success:', data);"
                                "alert('Message sent successfully!');"
                                "}).catch((error) => {"
                                "console.error('Error:', error);"
                                "});"
                                "});"
                                "</script>"
                                "</body></html>";

    send(client_socket, html_response, strlen(html_response), 0); // Send HTML response
}
