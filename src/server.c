#include "server.h"
#include <signal.h>
#include <errno.h>
#include <sys/signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define RELOAD_DELAY_MS 300
#define SHUTDOWN_TIMEOUT_SEC 5 

typedef struct {
    ws_clients_t* clients;
    bool* file_changed;
    pthread_mutex_t* mutex;
} ws_monitor_args_t;

volatile sig_atomic_t server_running = 1;
volatile sig_atomic_t shutdown_in_progress = 0;
pthread_t watcher_thread = 0;
pthread_t monitor_thread = 0;
ws_clients_t* ws_clients = NULL;
pthread_mutex_t* file_mutex_ptr = NULL;
ws_monitor_args_t* monitor_args = NULL;

void cleanup_resources(void);

void signal_handler(int signum) {
    if (shutdown_in_progress) {
        printf("\n%s%s[SERVER] %s%sForced shutdown - terminating immediately%s\n", 
                BOLD, COLOR_RED, BOLD, COLOR_YELLOW, COLOR_RESET);
        
        _exit(EXIT_FAILURE);
    }
    
    shutdown_in_progress = 1;
    server_running = 0;
    
    printf("\n%s%s[SERVER] %sReceived signal %d. Initiating shutdown sequence...%s\n", 
           BOLD, COLOR_BLUE, COLOR_YELLOW, signum, COLOR_RESET);
    printf("%s%s[SERVER] %sPlease wait for cleanup to complete (press Ctrl+C again for forced exit)%s\n", 
           BOLD, COLOR_BLUE, COLOR_CYAN, COLOR_RESET);
    
    struct sigaction forced_exit;
    memset(&forced_exit, 0, sizeof(forced_exit));
    forced_exit.sa_handler = signal_handler;
    sigaction(SIGINT, &forced_exit, NULL);
    sigaction(SIGTERM, &forced_exit, NULL);
}

void* shutdown_watchdog(void* arg) {
    int timeout = SHUTDOWN_TIMEOUT_SEC;
    printf("%s%s[SERVER] %sShutdown watchdog started (timeout: %d seconds)%s\n", 
           BOLD, COLOR_BLUE, COLOR_CYAN, timeout, COLOR_RESET);
    
    sleep(timeout);
    if (shutdown_in_progress) {
        printf("%s%s[SERVER] %s%sShutdown timeout exceeded! Forcing exit...%s\n", 
               BOLD, COLOR_RED, BOLD, COLOR_YELLOW, COLOR_RESET);
        _exit(EXIT_FAILURE);
    }
    
    return NULL;
}

void* ws_monitor_thread(void* args) {
    ws_monitor_args_t* monitor_args = (ws_monitor_args_t*)args;
    bool last_change_state = false;
    time_t last_reload_time = 0;
    const int RELOAD_COOLDOWN_SEC = 1; 
    const int PING_INTERVAL_MS = 10000; 
    time_t last_ping_time = 0;
    
    printf("%s%s[WS MONITOR] %sThread started%s\n", 
           BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_RESET);
    
    while (server_running) {
        time_t current_time = time(NULL);
        bool should_notify = false;
        if (difftime(current_time, last_ping_time) * 1000 >= PING_INTERVAL_MS) {
            if (monitor_args->clients && monitor_args->clients->count > 0) {
                broadcast_to_ws_clients(monitor_args->clients, "ping");
                last_ping_time = current_time;
            }
        }
        
        pthread_mutex_lock(monitor_args->mutex);
        bool current_change_state = *(monitor_args->file_changed);
        
        if (current_change_state) {
            if (difftime(current_time, last_reload_time) >= RELOAD_COOLDOWN_SEC) {
                printf("%s%s[HOT RELOAD] %sFile changes detected, preparing notification%s\n", 
                       BOLD, COLOR_MAGENTA, COLOR_RESET, COLOR_RESET);
                
                if (monitor_args->clients && monitor_args->clients->count > 0) {
                    should_notify = true;
                    last_reload_time = current_time;
                } else {
                    printf("%s%s[HOT RELOAD] %sNo clients connected, skipping notification%s\n", 
                           BOLD, COLOR_MAGENTA, COLOR_YELLOW, COLOR_RESET);
                }
                
                *(monitor_args->file_changed) = false;
            } else {
                printf("%s%s[HOT RELOAD] %sChanges detected during cooldown period (%ds), deferring%s\n", 
                       BOLD, COLOR_MAGENTA, COLOR_YELLOW, RELOAD_COOLDOWN_SEC, COLOR_RESET);
            }
        }
        
        pthread_mutex_unlock(monitor_args->mutex);
        
        if (should_notify) {
            usleep(RELOAD_DELAY_MS * 1000);
            printf("%s%s[HOT RELOAD] %sNotifying %s%d%s client(s) to reload%s\n", 
                   BOLD, COLOR_MAGENTA, COLOR_RESET, 
                   COLOR_YELLOW, monitor_args->clients->count, COLOR_RESET, COLOR_RESET);
            
            broadcast_to_ws_clients(monitor_args->clients, "reload");
            
            printf("%s%s[HOT RELOAD] %s%sReload notification sent successfully%s\n", 
                   BOLD, COLOR_MAGENTA, BOLD, COLOR_GREEN, COLOR_RESET);
        }
        
        last_change_state = current_change_state;
        usleep(100000);
    }
    
    printf("%s%s[WS MONITOR] %s%sThread stopped%s\n", 
           BOLD, COLOR_BLUE, BOLD, COLOR_YELLOW, COLOR_RESET);
    return NULL;
}

void cleanup_resources(void) {
    static int cleanup_running = 0;
    if (cleanup_running) {
        return;
    }
    
    cleanup_running = 1;
    printf("%s%s[SERVER] %sShutdown in progress, cleaning up resources...%s\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);
    pthread_t watchdog_thread;
    if (!pthread_create(&watchdog_thread, NULL, shutdown_watchdog, NULL)) {
        pthread_detach(watchdog_thread);
    }
    
    server_running = 0;
    if (ws_clients) {
        printf("%s%s[SERVER] %sClosing WebSocket connections...%s\n", 
              BOLD, COLOR_BLUE, COLOR_CYAN, COLOR_RESET);
        free_ws_clients(ws_clients);
        ws_clients = NULL;
    }
    
    if (watcher_thread != 0) {
        printf("%s%s[SERVER] %sStopping file watcher thread...%s\n", 
              BOLD, COLOR_BLUE, COLOR_CYAN, COLOR_RESET);
        int cancel_result = pthread_cancel(watcher_thread);
        if (cancel_result != 0) {
            fprintf(stderr, "%s%s[ERROR] %sFailed to cancel file watcher thread (error %d)%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, cancel_result, COLOR_RESET);
        }
        int join_result = pthread_join(watcher_thread, NULL);
        if (join_result != 0) {
            fprintf(stderr, "%s%s[ERROR] %sFailed to join file watcher thread (error %d)%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, join_result, COLOR_RESET);
            pthread_detach(watcher_thread);
        } else {
            printf("%s%s[SERVER] %sFile watcher thread stopped%s\n", 
                   BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_RESET);
        }
        watcher_thread = 0;
    }
    
    if (monitor_thread != 0) {
        printf("%s%s[SERVER] %sStopping WebSocket monitor thread...%s\n", 
              BOLD, COLOR_BLUE, COLOR_CYAN, COLOR_RESET);
        int cancel_result = pthread_cancel(monitor_thread);
        if (cancel_result != 0) {
            fprintf(stderr, "%s%s[ERROR] %sFailed to cancel WebSocket monitor thread (error %d)%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, cancel_result, COLOR_RESET);
        }
        int join_result = pthread_join(monitor_thread, NULL);
        if (join_result != 0) {
            fprintf(stderr, "%s%s[ERROR] %sFailed to join WebSocket monitor thread (error %d)%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, join_result, COLOR_RESET);
            pthread_detach(monitor_thread);
        } else {
            printf("%s%s[SERVER] %sWebSocket monitor thread stopped%s\n", 
                   BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_RESET);
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
    
    printf("%s%s[SERVER] %s%sCleanup complete%s\n", 
           BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET);
    cleanup_running = 0;
    shutdown_in_progress = 0;
}

int main(int argc, char *argv[]) {
    int server_fd = -1, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    bool file_changed = false;
    pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
    file_mutex_ptr = &file_mutex;
    int port = PORT; 
    char* custom_html_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                int custom_port = atoi(argv[i + 1]);
                if (custom_port > 0 && custom_port < 65536) {
                    port = custom_port;
                    i++;
                } else {
                    fprintf(stderr, "%s%s[CONFIG] %sInvalid port number. Using default port %d%s\n", 
                            BOLD, COLOR_YELLOW, COLOR_RESET, PORT, COLOR_RESET);
                }
            }
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--no-templates") == 0) {
            set_template_settings(false);
            printf("%s%s[CONFIG] %sTemplate processing disabled%s\n", 
                   BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_RESET);
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--serve") == 0) {
            if (i + 1 < argc) {
                custom_html_file = argv[i + 1];
                set_custom_html_file(custom_html_file);
                printf("%s%s[CONFIG] %sUsing custom HTML file: %s%s%s\n", 
                       BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_CYAN, custom_html_file, COLOR_RESET);
                i++;
            } else {
                fprintf(stderr, "%s%s[CONFIG] %sNo file specified after -s/--serve option%s\n", 
                        BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_RESET);
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("%s%s[HELP]%s Usage: %s [OPTIONS]\n", BOLD, COLOR_BLUE, COLOR_RESET, argv[0]);
            printf("Options:\n");
            printf("  -p, --port PORT      Specify port number (default: %d)\n", PORT);
            printf("  -s, --serve FILE     Specify a custom HTML file to serve\n");
            printf("  -n, --no-templates   Disable template processing\n");
            printf("  -h, --help           Display this help message\n");
            return EXIT_SUCCESS;
        }
    }
    
    set_server_port(port);   
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    
    ws_clients = init_ws_clients();
    if (!ws_clients) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to initialize WebSocket clients%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        cleanup_resources();
        return EXIT_FAILURE;
    }

    if (init_file_watcher(HTML_DIR, &file_changed, &file_mutex) != 0) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to initialize file watcher%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        cleanup_resources();
        return EXIT_FAILURE;
    }

    watcher_thread = start_file_watcher(HTML_DIR, &file_changed, &file_mutex);
    if (watcher_thread == 0) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to start file watcher thread%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    monitor_args = malloc(sizeof(ws_monitor_args_t));
    if (!monitor_args) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to allocate memory for WebSocket monitor: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        cleanup_resources();
        return EXIT_FAILURE;
    }
    
    monitor_args->clients = ws_clients;
    monitor_args->file_changed = &file_changed;
    monitor_args->mutex = &file_mutex;
    if (pthread_create(&monitor_thread, NULL, ws_monitor_thread, monitor_args) != 0) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to create WebSocket monitor thread: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
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

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "%s%s[WARNING] %sError setting SO_REUSEADDR option: %s%s\n", 
                BOLD, COLOR_YELLOW, COLOR_RESET, strerror(errno), COLOR_RESET);
    }
    
    printf("\n");
    printf("%s%s┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃                                               ┃%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %s _____ _ _     _      ___     ___   ___      %s┃%s\n", BOLD, COLOR_GREEN, COLOR_WHITE, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %s| __  | |_|___| |_   |_  |   |   | |   |     %s┃%s\n", BOLD, COLOR_GREEN, COLOR_WHITE, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %s| __ -| | |   | '_|   _| |_ _| | |_| | |     %s┃%s\n", BOLD, COLOR_GREEN, COLOR_WHITE, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %s|_____|_|_|_|_|_,_|  |_____|_|___|_|___|     %s┃%s\n", BOLD, COLOR_GREEN, COLOR_WHITE, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃                                               ┃%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %sS E R V E R   S T A R T E D   %s               ┃%s\n", BOLD, COLOR_GREEN, COLOR_WHITE, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃                                               ┃%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %sPORT:%s %-37d  ┃%s\n", BOLD, COLOR_GREEN, COLOR_YELLOW, COLOR_CYAN, port, COLOR_RESET);
    printf("%s%s┃                                               ┃%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %sHOT RELOAD:%s %-31s  ┃%s\n", BOLD, COLOR_GREEN, COLOR_YELLOW, COLOR_CYAN, "ENABLED", COLOR_RESET);
    printf("%s%s┃                                               ┃%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┃  %sTEMPLATES:%s %-32s  ┃%s\n", BOLD, COLOR_GREEN, COLOR_YELLOW, COLOR_CYAN, enable_templates ? "ENABLED" : "DISABLED", COLOR_RESET);
    printf("%s%s┃                                               ┃%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("%s%s┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛%s\n", BOLD, COLOR_GREEN, COLOR_RESET);
    printf("\n");
    printf("%s%s[SERVER] %sPress Ctrl+C to stop the server%s\n", BOLD, COLOR_BLUE, COLOR_RESET, COLOR_RESET);

    fd_set read_fds;
    struct timeval timeout;
    int max_fd = server_fd;
    time_t last_ping_time = 0;
    const int PING_INTERVAL_MS = 5000;

    while (server_running) {
        if (!server_running) {
            printf("%s%s[SERVER] %sShutdown signal received, exiting main loop...%s\n", 
                   BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_RESET);
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
            fprintf(stderr, "%s%s[ERROR] %sSelect error: %s%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
            break;
        }
        
        time_t current_time = time(NULL);
        if (ws_clients && server_running && ws_clients->count > 0 && 
            difftime(current_time, last_ping_time) * 1000 >= PING_INTERVAL_MS) {
            broadcast_to_ws_clients(ws_clients, "ping");
            last_ping_time = current_time;
        }
        
        if (activity > 0 && FD_ISSET(server_fd, &read_fds) && server_running) {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                fprintf(stderr, "%s%s[ERROR] %sConnection not accepted: %s%s\n", 
                        BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
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
        server_fd = -1;
    }
    cleanup_resources();  
    printf("%s%s[SERVER] %s%sServer shut down gracefully%s\n", 
           BOLD, COLOR_BLUE, BOLD, COLOR_GREEN, COLOR_RESET);
    return 0;
}
