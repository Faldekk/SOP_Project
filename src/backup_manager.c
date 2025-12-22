#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "backup_manager.h"
#include "monitor.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// creatin the manager struct thing
backup_manager_t* create_backup_manager() {
    backup_manager_t *mgr = calloc(1, sizeof(backup_manager_t));
    if (!mgr)
        return NULL;

    mgr->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (mgr->inotify_fd == -1) {
        free(mgr);
        return NULL;
    }

    mgr->head = NULL;
    return mgr;
}

// freein everything, dont forget to call this
void destroy_backup_manager(backup_manager_t *mgr) {
    if (!mgr)
        return;

    backup_entry_t *cur = mgr->head;
    while (cur) {
        backup_entry_t *next = cur->next;
        free(cur->source_path);
        free(cur->target_path);
        free(cur);
        cur = next;
    }

    if (mgr->inotify_fd != -1)
        close(mgr->inotify_fd);

    free(mgr);
}

// checcs if backup alredy exists in linked list
static int backup_exists(backup_manager_t *mgr, const char *source, const char *target) {
    for (backup_entry_t *c = mgr->head; c; c = c->next) {
        if (strcmp(c->source_path, source) == 0 && strcmp(c->target_path, target) == 0)
            return 1;
    }
    return 0;
}

// addin new backup and startin worker proces with fork
int add_backup(backup_manager_t *mgr, const char *source, const char *target) {
    if (!mgr || !source || !target)
        return -1;

    if (backup_exists(mgr, source, target)) {
        fprintf(stderr, "Backup already exists: %s -> %s\n", source, target);
        return -1;
    }

    backup_entry_t *entry = calloc(1, sizeof(backup_entry_t));
    if (!entry)
        return -1;

    entry->source_path = strdup(source);
    entry->target_path = strdup(target);
    entry->worker_pid = -1;
    entry->inotify_wd = -1;

    entry->next = mgr->head;
    mgr->head = entry;

    pid_t pid = fork();
    if (pid == 0) {
        start_backup_worker(source, target);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) {
        entry->worker_pid = pid;
        return 0;
    } else {
        ERR("fork - backup_manager.c");
        return -1;
    }
}

// killin worker and removin from list
int remove_backup(backup_manager_t *mgr, const char *source, const char *target) {
    if (!mgr || !source || !target)
        return -1;

    backup_entry_t *prev = NULL;
    backup_entry_t *cur = mgr->head;

    while (cur) {
        if (strcmp(cur->source_path, source) == 0 && strcmp(cur->target_path, target) == 0) {
            if (cur->worker_pid > 0) {
                sigset_t set, oldset;
                sigemptyset(&set);
                sigaddset(&set, SIGTERM);
                sigaddset(&set, SIGINT);
                sigprocmask(SIG_BLOCK, &set, &oldset);
                
                kill(cur->worker_pid, SIGTERM);
                waitpid(cur->worker_pid, NULL, 0);
                
                sigprocmask(SIG_SETMASK, &oldset, NULL);
                cur->worker_pid = -1;
            }

            if (cur->inotify_wd != -1 && mgr->inotify_fd != -1)
                inotify_rm_watch(mgr->inotify_fd, cur->inotify_wd);

            if (prev)
                prev->next = cur->next;
            else
                mgr->head = cur->next;

            free(cur->source_path);
            free(cur->target_path);
            free(cur);
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }

    fprintf(stderr, "Backup not found: %s -> %s\n", source, target);
    return -1;
}

// printin all backups
void list_backups(backup_manager_t *mgr) {
    if (!mgr)
        return;

    if (!mgr->head) {
        printf("No active backups.\n");
        return;
    }

    int idx = 1;
    for (backup_entry_t *c = mgr->head; c; c = c->next) {
        printf("%d. %s -> %s (PID: %d)\n", idx++, c->source_path, c->target_path, c->worker_pid);
    }
}

// killin all workers when exitin program
void kill_all_workers(backup_manager_t *mgr) {
    if (!mgr) return;

    for (backup_entry_t *c = mgr->head; c; c = c->next) {
        if (c->worker_pid > 0) {
            kill(c->worker_pid, SIGTERM);
            waitpid(c->worker_pid, NULL, 0);
            c->worker_pid = -1;
        }
        if (c->inotify_wd != -1 && mgr->inotify_fd != -1)
            inotify_rm_watch(mgr->inotify_fd, c->inotify_wd);
    }
}
