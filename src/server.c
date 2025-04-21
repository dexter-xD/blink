#include "server.h"
#include <signal.h>
#include <errno.h>
#include <sys/signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define RELOAD_DELAY_MS 300


typedef struct {
    ws_clients_t* clients;
    bool* file_changed;
    pthread_mutex_t* mutex;
} ws_monitor_args_t;

volatile sig_atomic_t server_running = 1;
pthread_t watcher_thread = 0;
pthread_t monitor_thread = 0;
ws_clients_t* ws_clients = NULL;
pthread_mutex_t* file_mutex_ptr = NULL;
ws_monitor_args_t* monitor_args = NULL;

void signal_handler(int signum) {
    static int shutdown_in_progress = 0;
    
    if (shutdown_in_progress) {
        if (signum == SIGINT) {
            printf("\nForced exit. Cleanup may be incomplete.\n");
            exit(EXIT_FAILURE);
        }
        return;
    }
    
    shutdown_in_progress = 1;
    server_running = 0;
    printf("\nReceived signal %d. Shutting down server...\n", signum);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}

void* ws_monitor_thread(void* args) {
    ws_monitor_args_t* monitor_args = (ws_monitor_args_t*)args;
    bool last_change_state = false;
    time_t last_reload_time = 0;
    const int RELOAD_COOLDOWN_SEC = 2;
    
    printf("WebSocket monitor thread started\n");
    
    while (server_running) {
        pthread_mutex_lock(monitor_args->mutex);
        bool current_change_state = *(monitor_args->file_changed);
        
        if (current_change_state && !last_change_state) {
            time_t current_time = time(NULL);
            if (difftime(current_time, last_reload_time) >= RELOAD_COOLDOWN_SEC) {
                printf("Hot reload: Detected file changes, notifying clients...\n");
                usleep(RELOAD_DELAY_MS * 1000);
                if (monitor_args->clients) {
                    broadcast_to_ws_clients(monitor_args->clients, "reload");
                }
                last_reload_time = current_time;
                *(monitor_args->file_changed) = false;
            } else {
                printf("Hot reload: Changes detected, but in cooldown period. Skipping...\n");
            }
        }
        
        last_change_state = *(monitor_args->file_changed);
        pthread_mutex_unlock(monitor_args->mutex);
        usleep(100000);
    }
    
    printf("WebSocket monitor thread exiting\n");
    return NULL;
}

void cleanup_resources() {
    static int cleanup_in_progress = 0;
    if (cleanup_in_progress) {
        return;
    }
    
    cleanup_in_progress = 1;
    printf("Cleaning up resources...\n");
    server_running = 0;
    if (watcher_thread != 0) {
        int cancel_result = pthread_cancel(watcher_thread);
        if (cancel_result != 0) {
            fprintf(stderr, "Warning: Failed to cancel file watcher thread (error %d)\n", cancel_result);
        }
        int join_result = pthread_join(watcher_thread, NULL);
        if (join_result != 0) {
            fprintf(stderr, "Warning: Failed to join file watcher thread (error %d)\n", join_result);
            pthread_detach(watcher_thread);
        } else {
            printf("File watcher thread stopped\n");
        }
        watcher_thread = 0;
    }
    
    if (monitor_thread != 0) {
        int cancel_result = pthread_cancel(monitor_thread);
        if (cancel_result != 0) {
            fprintf(stderr, "Warning: Failed to cancel WebSocket monitor thread (error %d)\n", cancel_result);
        }
        int join_result = pthread_join(monitor_thread, NULL);
        if (join_result != 0) {
            fprintf(stderr, "Warning: Failed to join WebSocket monitor thread (error %d)\n", join_result);
            pthread_detach(monitor_thread);
        } else {
            printf("WebSocket monitor thread stopped\n");
        }
        monitor_thread = 0;
    }
    
    if (monitor_args) {
        free(monitor_args);
        monitor_args = NULL;
    }
    
    if (file_mutex_ptr) {
        pthread_mutex_destroy(file_mutex_ptr);
        file_mutex_ptr = NULL;
    }
    
    if (ws_clients) {
        free_ws_clients(ws_clients);
        ws_clients = NULL;
    }
    
    printf("Cleanup complete\n");
    cleanup_in_progress = 0;
}

int main(int argc, char *argv[]) {
    int server_fd = -1, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    bool file_changed = false;
    pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
    file_mutex_ptr = &file_mutex;
    int port = PORT; 
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                int custom_port = atoi(argv[i + 1]);
                if (custom_port > 0 && custom_port < 65536) {
                    port = custom_port;
                    i++;
                } else {
                    fprintf(stderr, "Invalid port number. Using default port %d\n", PORT);
                }
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("Options:\n");
            printf("  -p, --port PORT    Specify port number (default: %d)\n", PORT);
            printf("  -h, --help         Display this help message\n");
            return EXIT_SUCCESS;
        }
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    ws_clients = init_ws_clients();
    if (!ws_clients) {
        fprintf(stderr, "Failed to initialize WebSocket clients\n");
        cleanup_resources();
        return EXIT_FAILURE;
    }

    if (init_file_watcher(HTML_DIR, &file_changed, &file_mutex) != 0) {
        fprintf(stderr, "Failed to initialize file watcher\n");
        cleanup_resources();
        return EXIT_FAILURE;
    }

    watcher_thread = start_file_watcher(HTML_DIR, &file_changed, &file_mutex);
    if (watcher_thread == 0) {
        fprintf(stderr, "Failed to start file watcher thread\n");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    monitor_args = malloc(sizeof(ws_monitor_args_t));
    if (!monitor_args) {
        perror("Failed to allocate memory for WebSocket monitor args");
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    monitor_args->clients = ws_clients;
    monitor_args->file_changed = &file_changed;
    monitor_args->mutex = &file_mutex;
    
    if (pthread_create(&monitor_thread, NULL, ws_monitor_thread, monitor_args) != 0) {
        perror("Failed to create WebSocket monitor thread");
        cleanup_resources();
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    server_fd = initialize_server(&address);
    if (server_fd == -1) {
        cleanup_resources();
        return EXIT_FAILURE;
    }

    printf("Server listening on port: %d\n", port);
    printf("Hot reload enabled, watching directory: %s\n", HTML_DIR);
    printf("Press Ctrl+C to stop the server\n");

    fd_set read_fds;
    struct timeval timeout;
    int max_fd = server_fd;

    while (server_running) {
        if (!server_running) {
            printf("Shutdown signal received, exiting main loop...\n");
            break;
        }
        
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        timeout.tv_sec = 0; 
        timeout.tv_usec = 100000;
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (!server_running) {
            break;
        }
        
        if (activity < 0) {
            if (errno == EINTR) {
                if (!server_running) {
                    break;
                }
                continue;
            }
            perror("Select error");
            break;
        }
        if (ws_clients) {
            broadcast_to_ws_clients(ws_clients, "ping");
        }
        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                perror("Connection not accepted!");
                continue;
            }
            char buffer[BUFFER_SIZE] = {0};
            ssize_t bytes_read = recv(new_socket, buffer, BUFFER_SIZE, MSG_PEEK);
            if (bytes_read > 0) {
                buffer[bytes_read < BUFFER_SIZE ? bytes_read : BUFFER_SIZE-1] = '\0';
                if (is_websocket_request(buffer)) {
                    handle_websocket_client(new_socket, ws_clients);
                } else {
                    handle_client(new_socket);
                }
            } else {
                close(new_socket);
            }
        }
    }
    if (server_fd != -1) {
        close(server_fd);
    }
    cleanup_resources();
    
    printf("Server shut down gracefully\n");
    return 0;
}
