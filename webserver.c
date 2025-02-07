#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void send_html(int client_socket, const char *file_path) {
  FILE *html_file = fopen(file_path, "r");
  if (html_file == NULL) {
    perror("Error opening HTML file");
    return;
  }

  char buffer[BUFFER_SIZE];
  size_t bytes_read;

  // Send HTTP header
  char *http_header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  send(client_socket, http_header, strlen(http_header), 0);

  // Send HTML file content
  while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, html_file)) > 0) {
    send(client_socket, buffer, bytes_read, 0);
  }

  fclose(html_file);
}

int main() {
  int server_socket, client_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);

  // Create socket
  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Bind socket
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(server_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("Bind failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(server_socket, 5) < 0) {
    perror("Listen failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d...\n", PORT);

  while (1) {
    // Accept incoming connection
    client_socket =
        accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket < 0) {
      perror("Accept failed");
      continue;
    }

    printf("Client connected\n");

    // Serve the HTML file
    send_html(client_socket, "index.html");

    // Close the client socket
    close(client_socket);
    printf("Client disconnected\n");
  }

  // Close the server socket (this will never be reached in this example)
  close(server_socket);

  return 0;
}
