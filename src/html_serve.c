#include "html_serve.h"

// Function to read an HTML file and return its contents
char* serve_html(const char* filename) {
    // Open the HTML file in read mode
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file"); // Display error if file can't be opened
        return NULL;
    }

    // Move file pointer to the end to determine the file size
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error seeking to end of file"); // Display error if seek fails
        fclose(file);                           // Close the file
        return NULL;
    }

    // Get the size of the file
    long length = ftell(file);
    if (length == -1) {
        perror("Error getting file size"); // Display error if ftell fails
        fclose(file);                      // Close the file
        return NULL;
    }

    // Reset file pointer to the beginning of the file
    if (fseek(file, 0, SEEK_SET) != 0) {
        perror("Error seeking to beginning of file"); // Display error if seek fails
        fclose(file);                                 // Close the file
        return NULL;
    }

    // Allocate memory to store the contents of the file, plus a null terminator
    char* buffer = malloc(length + 1);
    if (!buffer) {
        perror("Error allocating memory"); // Display error if malloc fails
        fclose(file);                      // Close the file
        return NULL;
    }

    // Read the file contents into the buffer
    if (fread(buffer, 1, length, file) != length) {
        perror("Error reading file");                  // Display error if fread fails
        free(buffer);                                  // Free the allocated memory
        fclose(file);                                  // Close the file
        return NULL;
    }

    buffer[length] = '\0'; // Null-terminate the buffer to make it a valid string
    fclose(file);          // Close the file
    return buffer;         // Return the buffer containing the HTML content
}
