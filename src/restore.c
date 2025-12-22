#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include "restore.h"

// comparin files and copyin only if diferent
int compare_and_copy_if_different(const char *src, const char *dst) {
    struct stat st_src, st_dst;
    
    if (stat(src, &st_src) == -1) {
        return -1;
    }
    
    int need_copy = 1;
    if (stat(dst, &st_dst) == 0) {
        if (st_src.st_mtime == st_dst.st_mtime && st_src.st_size == st_dst.st_size) {
            need_copy = 0;
        }
    }
    
    if (need_copy) {
        if (copy_file(src, dst) != 0) {
            return -1;
        }
        fprintf(stdout, "Restored: %s\n", src);
    }
    return 0;
}

// makin sure directory exists, creates if not
static int ensure_directory_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (errno != ENOENT) return -1;

    char tmp[PATH_MAX];
    size_t len = strnlen(path, PATH_MAX - 1);
    if (len == 0 || len >= PATH_MAX) return -1;
    memcpy(tmp, path, len);
    tmp[len] = '\0';

    for (size_t i = 1; i <= len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\0') {
            char saved = tmp[i];
            tmp[i] = '\0';
            if (strlen(tmp) > 0) {
                if (mkdir(tmp, 0755) == -1 && errno != EEXIST) {
                    tmp[i] = saved;
                    return -1;
                }
            }
            tmp[i] = saved;
        }
    }
    return 0;
}

// deletin files that dont exist in baccup anymore
void delete_files_not_in_backup(const char *source, const char *target) {
    DIR *dir = opendir(source);
    if (!dir) {
        perror("opendir failed");
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char source_path[PATH_MAX];
        char target_path[PATH_MAX];
        snprintf(source_path, PATH_MAX, "%s/%s", source, entry->d_name);
        snprintf(target_path, PATH_MAX, "%s/%s", target, entry->d_name);
        
        struct stat st;
        if (lstat(target_path, &st) == -1) {
            if (lstat(source_path, &st) == -1) {
                continue;
            }
            
            if (S_ISDIR(st.st_mode)) {
                delete_files_not_in_backup(source_path, target_path);
                if (rmdir(source_path) == -1) {
                    perror("rmdir failed");
                }
            } else {
                if (unlink(source_path) == -1) {
                    perror("unlink failed");
                }
            }
        } else {
            if (lstat(source_path, &st) == -1) {
                continue;
            }
            if (S_ISDIR(st.st_mode)) {
                delete_files_not_in_backup(source_path, target_path);
            }
        }
    }
    closedir(dir);
}

// main restore function, copys baccup back to source
int restore_backup(const char *source, const char *target) {
    if (ensure_directory_exists(target) == -1) {
        fprintf(stderr, "Failed to create target directory for restore: %s\n", target);
        return -1;
    }

    DIR *dir = opendir(target);
    if (!dir) {
        perror("opendir failed");
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char source_path[PATH_MAX];
        char target_path[PATH_MAX];
        snprintf(source_path, PATH_MAX, "%s/%s", source, entry->d_name);
        snprintf(target_path, PATH_MAX, "%s/%s", target, entry->d_name);
        
        struct stat st;
        if (lstat(target_path, &st) == -1) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            if (mkdir(source_path, st.st_mode & 0777) == -1 && errno != EEXIST) {
                perror("mkdir failed");
            }
            restore_backup(source_path, target_path);
        } else if (S_ISLNK(st.st_mode)) {
            char link_buf[PATH_MAX];
            ssize_t len = readlink(target_path, link_buf, PATH_MAX - 1);
            if (len == -1) {
                perror("readlink failed");
                continue;
            }
            link_buf[len] = '\0';
            
            if (unlink(source_path) == -1 && errno != ENOENT) {
                perror("unlink failed");
            }
            
            if (symlink(link_buf, source_path) == -1) {
                perror("symlink failed");
            }
        } else {
            if (compare_and_copy_if_different(target_path, source_path) != 0) {
                fprintf(stderr, "Failed to restore file: %s\n", target_path);
            }
        }
    }
    closedir(dir);
    
    delete_files_not_in_backup(source, target);
    
    fprintf(stdout, "Restore from %s to %s completed\n", target, source);
    return 0;
}
