/*
 * monitor.h - Deklaracje funkcji monitorowania i tworzenia backupów
 * 
 * Zawiera funkcje do początkowego backupu, sprawdzania pustych katalogów,
 * rekurencyjnego inotify watch i obsługi eventów.
 */

#ifndef MONITOR_H
#define MONITOR_H

#include <sys/types.h>
#include <sys/inotify.h>

typedef struct {
    char *source_path;
    char *target_path;
} backup_paths_t;

void start_backup_worker(const char *source, const char *target);
int create_initial_backup(const char *source, const char *target);
int is_directory_empty(const char *path);
int add_watch_recursive(int inotify_fd, const char *path);
void handle_inotify_event(struct inotify_event *event, const char *source, const char *target, int inotify_fd);

#endif
