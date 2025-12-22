
#ifndef MONITOR_H
#define MONITOR_H

#include <sys/inotify.h>
#include <sys/types.h>

typedef struct
{
    char *source_path;
    char *target_path;
} backup_paths_t;

void start_backup_worker(const char *source, const char *target);
int create_initial_backup(const char *source, const char *target);
int add_watch_recursive(int inotify_fd, const char *path);
void handle_inotify_event(struct inotify_event *event, const char *source, const char *target, int inotify_fd);

#endif
