#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#include <sys/types.h>

typedef struct backup_entry {
    char *source_path;
    char *target_path;
    pid_t worker_pid;
    int inotify_wd;
    struct backup_entry *next;
} backup_entry_t;

typedef struct {
    backup_entry_t *head;
    int inotify_fd;
} backup_manager_t;

backup_manager_t* create_backup_manager();
void destroy_backup_manager(backup_manager_t *mgr);

int add_backup(backup_manager_t *mgr, const char *source, const char *target);
int remove_backup(backup_manager_t *mgr, const char *source, const char *target);
void list_backups(backup_manager_t *mgr);
void kill_all_workers(backup_manager_t *mgr);

#endif
