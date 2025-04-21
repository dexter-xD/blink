#include "file_watcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

int init_file_watcher(const char* directory, bool* file_changed, pthread_mutex_t* mutex) {
    if (!directory || !file_changed || !mutex) {
        return -1;
    }

    struct stat st;
    if (stat(directory, &st) == -1) {
        perror("Error checking directory");
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s is not a directory\n", directory);
        return -1;
    }

    *file_changed = false;
    
    return 0;
}

// Start the file watcher in a separate thread
pthread_t start_file_watcher(const char* directory, bool* file_changed, pthread_mutex_t* mutex) {
    pthread_t thread_id;
    watcher_args_t* args = malloc(sizeof(watcher_args_t));
    
    if (!args) {
        perror("Failed to allocate memory for watcher args");
        return 0;
    }
    
    args->directory = strdup(directory);
    args->file_changed = file_changed;
    args->mutex = mutex;
    
    if (pthread_create(&thread_id, NULL, watch_files, args) != 0) {
        perror("Failed to create file watcher thread");
        free(args->directory);
        free(args);
        return 0;
    }
    
    return thread_id;
}

// Check if a file has been modified
bool file_has_changed(const char* filename, time_t* last_modified) {
    struct stat attr;
    if (stat(filename, &attr) == 0) {
        if (*last_modified == 0 || attr.st_mtime > *last_modified) {
            *last_modified = attr.st_mtime;
            return *last_modified != 0;
        }
    }
    return false;
}

// File watcher thread function
void* watch_files(void* args) {
    watcher_args_t* watcher_args = (watcher_args_t*)args;
    int fd, wd;
    char buffer[BUF_LEN];
    
    time_t last_notification_time = 0;
    const int DEBOUNCE_TIME_MS = 1000;
    
    fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        free(watcher_args->directory);
        free(watcher_args);
        return NULL;
    }
    
    wd = inotify_add_watch(fd, watcher_args->directory, IN_MODIFY | IN_CREATE | IN_CLOSE_WRITE);
    if (wd < 0) {
        perror("inotify_add_watch");
        close(fd);
        free(watcher_args->directory);
        free(watcher_args);
        return NULL;
    }
    
    printf("Watching directory: %s\n", watcher_args->directory);
    
    while (1) {
        int length = read(fd, buffer, BUF_LEN);
        if (length < 0) {
            if (errno != EAGAIN) {
                perror("read");
                break;
            }
            continue;
        }
        
        if (length == 0) continue;
        
        int i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            
            if (event->len > 0 && (event->mask & (IN_MODIFY | IN_CREATE | IN_CLOSE_WRITE))) {
                char* dot = strrchr(event->name, '.');
                if (dot && (strcmp(dot, ".html") == 0)) {
                    time_t current_time = time(NULL);
                    
                    if (difftime(current_time, last_notification_time) * 1000 >= DEBOUNCE_TIME_MS) {
                        printf("File %s was modified\n", event->name);
                        
                        usleep(200000);
                        
                        pthread_mutex_lock(watcher_args->mutex);
                        *(watcher_args->file_changed) = true;
                        pthread_mutex_unlock(watcher_args->mutex);
                        
                        last_notification_time = current_time;
                    }
                }
            }
            
            i += EVENT_SIZE + event->len;
        }
        
        usleep(100000);
    }
    
    inotify_rm_watch(fd, wd);
    close(fd);
    free(watcher_args->directory);
    free(watcher_args);
    
    return NULL;
} 