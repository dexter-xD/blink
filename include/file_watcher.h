#ifndef FILE_WATCHER_H
#define FILE_WATCHER_H

#include <sys/inotify.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

typedef struct {
    char* path;
    time_t last_modified;
} file_info_t;

typedef struct {
    char* directory;
    bool* file_changed;
    pthread_mutex_t* mutex;
} watcher_args_t;

int init_file_watcher(const char* directory, bool* file_changed, pthread_mutex_t* mutex);
pthread_t start_file_watcher(const char* directory, bool* file_changed, pthread_mutex_t* mutex);
bool file_has_changed(const char* filename, time_t* last_modified);
void* watch_files(void* args);

#endif