#include "socket_utils.h"
#include <errno.h>

int initialize_server(struct sockaddr_in* address) {
    int server_fd;
    int opt = 1;
    int retry_count = 0;
    const int MAX_RETRIES = 5;
    const int RETRY_DELAY_SEC = 2;
    int port = ntohs(address->sin_port);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed!");
        return -1;
    }

    // Set SO_REUSEADDR on all platforms
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(server_fd);
        return -1;
    }
    
#ifdef __linux__
    // SO_REUSEPORT is only set on Linux
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) != 0) {
        perror("setsockopt SO_REUSEPORT failed");
        // Continue anyway, this is not critical
    }
#endif

    address->sin_family = AF_INET;         
    address->sin_addr.s_addr = INADDR_ANY; 

    while (retry_count < MAX_RETRIES) {
        if (bind(server_fd, (struct sockaddr*)address, sizeof(*address)) < 0) {
            if (errno == EADDRINUSE) {
                printf("Port %d already in use. Waiting %d seconds before retry (%d/%d)...\n", 
                        port, RETRY_DELAY_SEC, retry_count + 1, MAX_RETRIES);
                sleep(RETRY_DELAY_SEC);
                retry_count++;
            } else {
                perror("Bind failed!");  
                close(server_fd);        
                return -1;
            }
        } else {break;}
    }
    
    if (retry_count >= MAX_RETRIES) {
        fprintf(stderr, "Failed to bind after %d attempts. Try manually killing the process using port %d or use a different port.\n", 
                MAX_RETRIES, port);
        close(server_fd);
        return -1;
    }
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed!");  
        close(server_fd);          
        return -1;
    }
    return server_fd;  
}

void read_client_data(int socket, char* buffer) {
    ssize_t read_value;
    while ((read_value = read(socket, buffer, BUFFER_SIZE)) > 0) {
        printf("Client: %s", buffer); 
        memset(buffer, 0, BUFFER_SIZE); 
    }
}
