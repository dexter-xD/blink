#include "request_handler.h"

void handle_client(int new_socket) {
    char buffer[BUFFER_SIZE] = { 0 }; // Buffer to store incoming data from client

    // Read the HTTP request from the client into the buffer
    read(new_socket, buffer, BUFFER_SIZE);

    // Log accepted connections, ignoring requests for favicon (for clarity)
    if (strstr(buffer, "GET /") != NULL && strstr(buffer, "GET /favicon.ico") == NULL) {
        printf("Connection accepted\n");
        printf("Request: %s\n", buffer); // Print the HTTP request
    }

    // Check if the request is for the favicon and ignore if true
    if (strstr(buffer, "GET /favicon.ico") != NULL) {
        close(new_socket); // Close connection if favicon is requested
        return;
    }

    // Serve the main HTML file (index.html)
    char* html_content = serve_html("../html/index.html");
    if (html_content) {
        const char* keys[] = { "user", "is_logged_in" };
        const char* values[] = { "Dexter", "1" }; // 1 means is_logged_in and empty string ("") for not_logged_in
        const char* loop_key = "item";
        const char* loop_values[] = { "Item 1", "Item 2", "Item 3", "Item 4" }; // Items to loop over

        char* processed_template = process_template(html_content, keys, values, 2, loop_key, loop_values, 4);



        // Send HTTP response headers for successful request
        write(new_socket, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n", 48);
        // Send the HTML content of the file
        write(new_socket, processed_template, strlen(processed_template));
        free(html_content);        // Free the memory allocated for HTML content
        free(processed_template);     // Free the memory allocated for processed HTML content
    }
    else {
        // If HTML file not found, send a 404 Not Found response
        const char* not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
        write(new_socket, not_found_response, strlen(not_found_response));
    }

    // Close the connection with the client
    close(new_socket);
}
