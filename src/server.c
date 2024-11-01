// src/server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Function to read an HTML file and return its contents
char *serve_html(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);
    return buffer;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = { 0 };

    // Initialize server socket
    // (your existing socket initialization code here...)

    // Accept new connection
    // (your existing connection handling code here...)

    // Read HTTP request from client
    read(new_socket, buffer, BUFFER_SIZE);
    printf("%s\n", buffer); // For debugging

    // Serve the HTML file
    char *html_content = serve_html("index.html");
    if (html_content) {
        // Send HTTP response
        write(new_socket, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n", 48);
        write(new_socket, html_content, strlen(html_content));
    }

    free(html_content);
    close(new_socket);
    close(server_fd);
    return 0;
}
