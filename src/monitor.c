#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include "monitor.h"
#include "backup.h"
#include "signals.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct watch_node {
    int wd;
    char path[PATH_MAX];
    struct watch_node *next;
} watch_node_t;

static watch_node_t *watch_list = NULL;

static const char* find_watch_path(int wd) {
    for (watch_node_t *n = watch_list; n; n = n->next) {
        if (n->wd == wd) return n->path;
    }
    return NULL;
}

static void remove_watch_entry(int wd) {
    watch_node_t *prev = NULL, *cur = watch_list;
    while (cur) {
        if (cur->wd == wd) {
            if (prev) prev->next = cur->next; else watch_list = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

static int register_watch(int inotify_fd, const char *path) {
    uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE;

    int wd = inotify_add_watch(inotify_fd, path, mask);
    if (wd == -1) {
        perror("Failed to add inotify watch");
        return -1;
    }

    watch_node_t *node = calloc(1, sizeof(watch_node_t));
    if (!node) {
        perror("Memory allocation failed");
        inotify_rm_watch(inotify_fd, wd);
        return -1;
    }
    
    node->wd = wd;
    strncpy(node->path, path, PATH_MAX - 1);
    node->path[PATH_MAX - 1] = '\0';
    node->next = watch_list;
    watch_list = node;

    return wd;
}

static int remove_path_recursive(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        if (errno == ENOENT) return 0;
        perror("lstat failed");
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) {
            if (errno == ENOENT) return 0;
            perror("opendir failed");
            return -1;
        }
        struct dirent *ent;
        char child[PATH_MAX];
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            int n = snprintf(child, PATH_MAX, "%s/%s", path, ent->d_name);
            if (n <= 0 || n >= PATH_MAX) {
                fprintf(stderr, "Path too long while removing: %s/%s\n", path, ent->d_name);
                closedir(dir);
                return -1;
            }
            if (remove_path_recursive(child) == -1) {
                closedir(dir);
                return -1;
            }
        }
        closedir(dir);
        if (rmdir(path) == -1 && errno != ENOENT) {
            perror("rmdir failed");
            return -1;
        }
    } else {
        if (unlink(path) == -1 && errno != ENOENT) {
            perror("unlink failed");
            return -1;
        }
    }
    return 0;
}

int create_initial_backup(const char *source, const char *target) {
    struct stat st;
    if (lstat(source, &st) == -1) {
        fprintf(stderr, "Error: Source path '%s' does not exist\n", source);
        return -1;
    }
    
    char *real_source = realpath(source, NULL);
    if (!real_source) {
        fprintf(stderr, "Error: Cannot resolve source path '%s'\n", source);
        return -1;
    }

    char target_buf[PATH_MAX];
    const char *target_to_check = target;
    if (target[0] != '/') {
        if (getcwd(target_buf, PATH_MAX) == NULL) {
            fprintf(stderr, "Error: Cannot get current directory\n");
            free(real_source);
            return -1;
        }
        size_t cwd_len = strlen(target_buf);
        size_t target_len = strlen(target);
        if (cwd_len + 1 + target_len < PATH_MAX) {
            snprintf(target_buf + cwd_len, PATH_MAX - cwd_len, "/%s", target);
            target_to_check = target_buf;
        }
    } else {
        strncpy(target_buf, target, PATH_MAX - 1);
        target_buf[PATH_MAX - 1] = '\0';
        target_to_check = target_buf;
    }

    size_t source_len = strlen(real_source);
    size_t target_check_len = strlen(target_to_check);
    if (strncmp(target_to_check, real_source, source_len) == 0 &&
        (target_check_len == source_len || target_to_check[source_len] == '/')) {
        fprintf(stderr, "Error: Cannot backup directory inside itself\n");
        free(real_source);
        return -1;
    }
    
    free(real_source);
    
    if (lstat(target, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: Target '%s' exists but is not a directory\n", target);
            return -1;
        }
        
        if (is_directory_empty(target) == 0) {
            fprintf(stdout, "Warning: Target directory '%s' is not empty, files may be overwritten\n", target);
        }
    }
    
    fprintf(stdout, "Creating initial backup from %s to %s...\n", source, target);
    
    if (copy_tree(source, target, source, target) != 0) {
        fprintf(stderr, "Error: Failed to create initial backup\n");
        return -1;
    }
    
    fprintf(stdout, "Initial backup completed\n");
    return 0;
}

int is_directory_empty(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        ERR("Failed to open directory");
    }
    
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            closedir(dir);
            return 0;
        }
    }
    
    closedir(dir);
    return 1;
}

int add_watch_recursive(int inotify_fd, const char *path) {
    if (register_watch(inotify_fd, path) == -1) {
        return -1;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        ERR("Failed to open directory for watching");
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        int path_size = strlen(path) + 1 + strlen(entry->d_name) + 1;
        char *full_path = malloc(path_size);
        if (full_path == NULL) {
            perror("Memory allocation failed");
            closedir(dir);
            return -1;
        }
        snprintf(full_path, path_size, "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == -1) {
            perror("Failed to get file status");
            free(full_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (add_watch_recursive(inotify_fd, full_path) == -1) {
                fprintf(stderr, "Failed to watch directory: %s\n", full_path);
                free(full_path);
                closedir(dir);
                return -1;
            }
        }
        free(full_path);
    }
    
    closedir(dir);
    return 0;
}

void handle_inotify_event(struct inotify_event *event, const char *root_source, const char *root_target, int inotify_fd) {
    if (event->len == 0) {
        if (event->mask & IN_IGNORED) {
            remove_watch_entry(event->wd);
        }
        return;
    }

    if (event->mask & IN_IGNORED) {
        remove_watch_entry(event->wd);
        return;
    }

    const char *base_source = find_watch_path(event->wd);
    if (!base_source) {
        base_source = root_source;
    }

    size_t base_len = strnlen(base_source, PATH_MAX - 1);
    size_t name_len = strnlen(event->name, PATH_MAX - 1);

    char source_path[PATH_MAX];
    char target_path[PATH_MAX];

    if (base_len + 1 + name_len >= PATH_MAX) {
        fprintf(stderr, "Error: Source path too long: %s/%s\n", base_source, event->name);
        return;
    }
    snprintf(source_path, PATH_MAX, "%s/%s", base_source, event->name);

    const char *target_base = root_target;
    size_t root_len = strnlen(root_source, PATH_MAX - 1);
    if (strncmp(base_source, root_source, root_len) == 0) {
        target_base = root_target;
        const char *suffix = base_source + root_len;
        if (*suffix) {
            if (strlen(root_target) + strlen(suffix) < PATH_MAX) {
                static char computed[PATH_MAX];
                snprintf(computed, PATH_MAX, "%s%s", root_target, suffix);
                target_base = computed;
            }
        }
    }

    if (strlen(target_base) + 1 + name_len >= PATH_MAX) {
        fprintf(stderr, "Error: Target path too long: %s/%s\n", target_base, event->name);
        return;
    }
    int tlen = snprintf(target_path, PATH_MAX, "%s/%s", target_base, event->name);
    if (tlen < 0 || tlen >= PATH_MAX) {
        fprintf(stderr, "Error: Target path too long: %s/%s\n", target_base, event->name);
        return;
    }

    if (event->mask & IN_CREATE) {
        struct stat st;
        if (lstat(source_path, &st) == -1) {
            if (errno == ENOENT) {
                return;
            }
            perror("Failed to get file status");
            return;
        }

        if (S_ISDIR(st.st_mode)) {
            if (mkdir(target_path, 0755) == -1 && errno != EEXIST) {
                perror("Failed to create backup directory");
                return;
            }
            if (add_watch_recursive(inotify_fd, source_path) == -1) {
                fprintf(stderr, "Failed to watch directory: %s\n", source_path);
            }
            if (copy_tree(source_path, target_path, root_source, root_target) != 0) {
                fprintf(stderr, "Failed to backup directory: %s\n", source_path);
            }
        } else {
            copy_file(source_path, target_path);
        }
    }

    if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
        struct stat st;
        if (stat(source_path, &st) == -1) {
            if (errno == ENOENT) {
                return;
            }
        }
        copy_file(source_path, target_path);
    }

    if (event->mask & IN_DELETE) {
        remove_path_recursive(target_path);
    }

    if (event->mask & IN_MOVED_FROM) {
        remove_path_recursive(target_path);
    }
}

void start_backup_worker(const char *source, const char *target) {
    setup_signal_handlers();

    int inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd == -1) {
        perror("Failed to initialize inotify");
        exit(EXIT_FAILURE);
    }

    if (add_watch_recursive(inotify_fd, source) == -1) {
        fprintf(stderr, "Failed to set up file monitoring\n");
        close(inotify_fd);
        exit(EXIT_FAILURE);
    }

    char *event_buffer = malloc(sizeof(struct inotify_event) + NAME_MAX + 1);
    if (!event_buffer) {
        perror("Memory allocation failed");
        close(inotify_fd);
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Monitoring: %s -> %s\n", source, target);

    struct stat st;
    while (!should_exit) {
        if (stat(source, &st) == -1) {
            fprintf(stdout, "Source directory no longer exists, stopping monitor\n");
            break;
        }

        ssize_t bytes_read = read(inotify_fd, event_buffer, sizeof(struct inotify_event) + NAME_MAX + 1);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);
                continue;
            }
            perror("Failed to read events");
            break;
        }

        size_t offset = 0;
        while (offset < (size_t)bytes_read) {
            struct inotify_event *event = (struct inotify_event *)(event_buffer + offset);
            handle_inotify_event(event, source, target, inotify_fd);
            offset += sizeof(struct inotify_event) + event->len;
        }
    }

    free(event_buffer);
    close(inotify_fd);
    exit(EXIT_SUCCESS);
}
