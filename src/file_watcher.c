#include "file_watcher.h"
#include "websocket.h" 
#include "request_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>

typedef struct {
    char** filenames;
    time_t* last_modified;
    int count;
    int capacity;
} html_files_t;

static html_files_t* html_files = NULL;

static html_files_t* init_html_files() {
    html_files_t* files = malloc(sizeof(html_files_t));
    if (!files) return NULL;
    
    files->capacity = 10; 
    files->count = 0;
    files->filenames = malloc(files->capacity * sizeof(char*));
    files->last_modified = malloc(files->capacity * sizeof(time_t));
    
    if (!files->filenames || !files->last_modified) {
        if (files->filenames) free(files->filenames);
        if (files->last_modified) free(files->last_modified);
        free(files);
        return NULL;
    }
    
    return files;
}

static void add_html_file(html_files_t* files, const char* filename) {
    if (!files || !filename) return;
    for (int i = 0; i < files->count; i++) {
        if (strcmp(files->filenames[i], filename) == 0) {
            return;
        }
    }
    
    if (files->count >= files->capacity) {
        int new_capacity = files->capacity * 2;
        char** new_filenames = realloc(files->filenames, new_capacity * sizeof(char*));
        time_t* new_last_modified = realloc(files->last_modified, new_capacity * sizeof(time_t));
        
        if (!new_filenames || !new_last_modified) {
            if (new_filenames) files->filenames = new_filenames;
            fprintf(stderr, "%s%s[WARNING] %sMemory allocation failed, continuing with existing capacity%s\n", 
                    BOLD, COLOR_YELLOW, COLOR_RESET, COLOR_RESET);
            return; 
        }
        
        files->filenames = new_filenames;
        files->last_modified = new_last_modified;
        files->capacity = new_capacity;
    }
    
    // Add the file
    files->filenames[files->count] = strdup(filename);
    files->last_modified[files->count] = 0;  
    files->count++;
    
    printf("%s%s[FILE WATCHER] %sAdded HTML file to watch: %s%s%s\n", 
           BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, filename, COLOR_RESET);
}

static void scan_directory(const char* directory, html_files_t* files) {
    DIR* dir = opendir(directory);
    if (!dir) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to open directory: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char* extension = strrchr(entry->d_name, '.');
        if (extension && strcmp(extension, ".html") == 0) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", directory, entry->d_name);
            add_html_file(files, full_path);
        }
    }
    
    closedir(dir);
}

static void add_custom_html_file_if_exists(html_files_t* files) {
    extern char* custom_html_file; 
    
    if (custom_html_file && *custom_html_file) {
        struct stat st;
        if (stat(custom_html_file, &st) == 0 && S_ISREG(st.st_mode)) {
            bool already_watching = false;
            for (int i = 0; i < files->count; i++) {
                if (strcmp(files->filenames[i], custom_html_file) == 0) {
                    already_watching = true;
                    break;
                }
            }
            
            if (!already_watching) {
                printf("%s%s[FILE WATCHER] %sAdding custom HTML file to watch: %s%s%s\n", 
                       BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, custom_html_file, COLOR_RESET);
                add_html_file(files, custom_html_file);
            }
        } else {
            fprintf(stderr, "%s%s[WARNING] %sCustom HTML file not found or not accessible: %s%s\n", 
                    BOLD, COLOR_YELLOW, COLOR_RESET, custom_html_file, COLOR_RESET);
        }
    }
}

static void free_html_files(html_files_t* files) {
    if (!files) return;
    
    for (int i = 0; i < files->count; i++) {
        free(files->filenames[i]);
    }
    
    free(files->filenames);
    free(files->last_modified);
    free(files);
}

int init_file_watcher(const char* directory, bool* file_changed, pthread_mutex_t* mutex) {
    if (!directory || !file_changed || !mutex) {
        return -1;
    }

    struct stat st;
    if (stat(directory, &st) == -1) {
        fprintf(stderr, "%s%s[ERROR] %sError checking directory: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s%s[ERROR] %s%s is not a directory%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, directory, COLOR_RESET);
        return -1;
    }

    *file_changed = false;
    
    html_files = init_html_files();
    if (!html_files) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to initialize HTML file tracking%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, COLOR_RESET);
        return -1;
    }
    
    scan_directory(directory, html_files);
    
    add_custom_html_file_if_exists(html_files);
    
    printf("%s%s[FILE WATCHER] %sInitialized for directory: %s%s%s (tracking %s%d%s HTML files)\n", 
           BOLD, COLOR_BLUE, COLOR_RESET, COLOR_CYAN, directory, COLOR_RESET, 
           COLOR_YELLOW, html_files->count, COLOR_RESET);
    
    return 0;
}

pthread_t start_file_watcher(const char* directory, bool* file_changed, pthread_mutex_t* mutex) {
    pthread_t thread_id;
    watcher_args_t* args = malloc(sizeof(watcher_args_t));
    
    if (!args) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to allocate memory for watcher args: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        return 0;
    }
    
    args->directory = strdup(directory);
    args->file_changed = file_changed;
    args->mutex = mutex;
    
    if (pthread_create(&thread_id, NULL, watch_files, args) != 0) {
        fprintf(stderr, "%s%s[ERROR] %sFailed to create file watcher thread: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        free(args->directory);
        free(args);
        return 0;
    }
    
    return thread_id;
}

bool file_has_changed(const char* filename, time_t* last_modified) {
    struct stat attr;
    if (stat(filename, &attr) == 0) {
        if (*last_modified == 0 || attr.st_mtime > *last_modified) {
            time_t old_time = *last_modified;
            *last_modified = attr.st_mtime;
            return (old_time != 0 && old_time != attr.st_mtime);
        }
    }
    return false;
}

static bool check_all_files_for_changes(html_files_t* files) {
    if (!files || files->count == 0) return false;
    
    static char* last_changed_file = NULL;
    static time_t last_change_time = 0;
    time_t current_time = time(NULL);
    const int SAME_FILE_DEBOUNCE_SECS = 5;
    
    bool any_changed = false;
    for (int i = 0; i < files->count; i++) {
        if (file_has_changed(files->filenames[i], &files->last_modified[i])) {
            bool is_repeat = (last_changed_file && strcmp(files->filenames[i], last_changed_file) == 0);
            bool within_debounce = (difftime(current_time, last_change_time) < SAME_FILE_DEBOUNCE_SECS);
            
            if (!is_repeat || !within_debounce) {
                printf("%s%s[FILE WATCHER] %sDetected change in file: %s%s%s\n", 
                       BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, 
                       files->filenames[i], COLOR_RESET);
                
                if (last_changed_file) {
                    free(last_changed_file);
                }
                last_changed_file = strdup(files->filenames[i]);
                last_change_time = current_time;
            }
            
            any_changed = true;
        }
    }
    
    return any_changed;
}

static void rescan_directory_if_needed(const char* directory, html_files_t* files) {
    static time_t last_scan_time = 0;
    time_t current_time = time(NULL);
    
    if (difftime(current_time, last_scan_time) > 30) {
        printf("%s%s[FILE WATCHER] %sRescanning directory for new HTML files...%s\n", 
               BOLD, COLOR_BLUE, COLOR_CYAN, COLOR_RESET);
        
        scan_directory(directory, files);
        add_custom_html_file_if_exists(files);
        
        last_scan_time = current_time;
    }
}

void* watch_files(void* args) {
    watcher_args_t* watcher_args = (watcher_args_t*)args;
    int fd, wd, custom_file_wd = -1;
    char buffer[BUF_LEN];
    extern char* custom_html_file; 
    
    time_t last_notification_time = 0;
    const int DEBOUNCE_TIME_MS = 1000;
    fd = inotify_init();
    if (fd < 0) {
        fprintf(stderr, "%s%s[ERROR] %sinotify_init failed: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        free(watcher_args->directory);
        free(watcher_args);
        return NULL;
    }
    
    wd = inotify_add_watch(fd, watcher_args->directory, 
                          IN_MODIFY | IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB);
    if (wd < 0) {
        fprintf(stderr, "%s%s[ERROR] %sinotify_add_watch failed: %s%s\n", 
                BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        close(fd);
        free(watcher_args->directory);
        free(watcher_args);
        return NULL;
    }
    
    if (custom_html_file && *custom_html_file) {
        char* last_slash = strrchr(custom_html_file, '/');
        if (last_slash) {
            char custom_dir[512] = {0};
            strncpy(custom_dir, custom_html_file, last_slash - custom_html_file);
            custom_dir[last_slash - custom_html_file] = '\0';
            
            if (strcmp(custom_dir, watcher_args->directory) != 0) {
                custom_file_wd = inotify_add_watch(fd, custom_dir, 
                                  IN_MODIFY | IN_CREATE | IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB);
                
                if (custom_file_wd >= 0) {
                    printf("%s%s[FILE WATCHER] %sAdded watch for custom HTML file directory: %s%s%s\n", 
                           BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, custom_dir, COLOR_RESET);
                }
            }
        }
    }
    
    printf("%s%s[FILE WATCHER] %sActive - watching directory: %s%s%s\n", 
           BOLD, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN, watcher_args->directory, COLOR_RESET);
    
    while (1) {
        bool change_detected = false;
        if (check_all_files_for_changes(html_files)) {
            change_detected = true;
        }
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);       
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        
        int ret = select(fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (ret > 0 && FD_ISSET(fd, &read_fds)) {
            int length = read(fd, buffer, BUF_LEN);
            if (length > 0) {
                int i = 0;
                while (i < length) {
                    struct inotify_event* event = (struct inotify_event*)&buffer[i];
                    
                    if (event->len > 0) {
                        char* dot = strrchr(event->name, '.');
                        if (dot && (strcmp(dot, ".html") == 0)) {
                            printf("%s%s[FILE WATCHER] %sEvent detected: %s%s%s (mask: 0x%08x)\n", 
                                   BOLD, COLOR_BLUE, COLOR_RESET, COLOR_CYAN, event->name, COLOR_RESET, event->mask);
                            change_detected = true;
                            char full_path[512];
                            snprintf(full_path, sizeof(full_path), "%s/%s", 
                                     watcher_args->directory, event->name);
                            add_html_file(html_files, full_path);
                        }
                    }
                    
                    i += EVENT_SIZE + event->len;
                }
            }
        } else if (ret < 0 && errno != EINTR) {
            fprintf(stderr, "%s%s[FILE WATCHER] %sSelect error: %s%s\n", 
                    BOLD, COLOR_RED, COLOR_RESET, strerror(errno), COLOR_RESET);
        }
        
        rescan_directory_if_needed(watcher_args->directory, html_files);
        if (change_detected) {
            time_t current_time = time(NULL);
            double ms_since_last = difftime(current_time, last_notification_time) * 1000;
            if (ms_since_last >= DEBOUNCE_TIME_MS) {
                bool already_signaled = false;
                pthread_mutex_lock(watcher_args->mutex);
                already_signaled = *(watcher_args->file_changed);
                pthread_mutex_unlock(watcher_args->mutex);
                
                if (!already_signaled) {
                    usleep(100000);                    
                    pthread_mutex_lock(watcher_args->mutex);
                    *(watcher_args->file_changed) = true;
                    pthread_mutex_unlock(watcher_args->mutex);                   
                    printf("%s%s[FILE WATCHER] %sSignaled main thread about file changes%s\n", 
                           BOLD, COLOR_BLUE, COLOR_YELLOW, COLOR_RESET);
                    last_notification_time = current_time;
                } else {
                    static time_t last_skip_message = 0;
                    if (difftime(current_time, last_skip_message) > 5) {
                        printf("%s%s[FILE WATCHER] %sSkipping notification - one already pending%s\n", 
                               BOLD, COLOR_BLUE, COLOR_CYAN, COLOR_RESET);
                        last_skip_message = current_time;
                    }
                }
            } else {
                static time_t last_debounce_message = 0;
                if (difftime(current_time, last_debounce_message) > 5) {
                    printf("%s%s[FILE WATCHER] %sDebouncing - %d ms since last notification%s\n", 
                           BOLD, COLOR_BLUE, COLOR_CYAN, (int)ms_since_last, COLOR_RESET);
                    last_debounce_message = current_time;
                }
            }
        }
        usleep(100000); 
    }
    
    if (custom_file_wd >= 0) {
        inotify_rm_watch(fd, custom_file_wd);
    }
    inotify_rm_watch(fd, wd);
    close(fd);
    
    free(watcher_args->directory);
    free(watcher_args);
    free_html_files(html_files);
    
    return NULL;
} 