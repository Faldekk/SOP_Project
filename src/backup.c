#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "backup.h"
#include "utils.h"
#include <dirent.h>
#include <limits.h>

#define FILE_BUF_LEN 65536

ssize_t bulk_read(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}
ssize_t bulk_write(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

int copy_file(const char* source_path, const char* dest_path)
{
    struct stat source_stat;
    if (stat(source_path, &source_stat) == -1) {
        if (errno == ENOENT) {
            return -1;
        }
        ERR("Failed to get source file info");
    }

    const int source_fd = open(source_path, O_RDONLY);
    if (source_fd == -1) {
        if (errno == ENOENT) {
            return -1;
        }
        ERR("Failed to open source file");
    }

    unlink(dest_path);

    const int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, source_stat.st_mode & 0777);
    if (dest_fd == -1) {
        ERR("Failed to create destination file");
    }

    char buffer[FILE_BUF_LEN];
    for (;;) {
        const ssize_t bytes_read = bulk_read(source_fd, buffer, FILE_BUF_LEN);
        if (bytes_read == -1) {
            ERR("Failed to read from source file");
        }
        if (bytes_read == 0) {
            break;
        }
        if (bulk_write(dest_fd, buffer, bytes_read) == -1) {
            ERR("Failed to write to destination file");
        }
    }

    close(source_fd);
    close(dest_fd);
    return EXIT_SUCCESS;
}


int copy_symlink_smart(const char *path_src, const char *path_dst, const char *source_base, const char *target_base) {
    char link_buf[PATH_MAX];
    ssize_t len = readlink(path_src, link_buf, PATH_MAX - 1);
    
    if (len == -1) {
        perror("readlink");
        return EXIT_FAILURE;
    }
    
    link_buf[len] = '\0';
    
    if (link_buf[0] == '/') {
        
        char *real_source_base = realpath(source_base, NULL);
        char *real_target_base = realpath(target_base, NULL);
        
        if (real_source_base && real_target_base) {
            size_t source_len = strlen(real_source_base);
            if (strncmp(link_buf, real_source_base, source_len) == 0 &&
                (link_buf[source_len] == '/' || link_buf[source_len] == '\0')) {
                char new_path[PATH_MAX];
                snprintf(new_path, PATH_MAX, "%s%s", real_target_base, link_buf + source_len);
                free(real_source_base);
                free(real_target_base);
                if (symlink(new_path, path_dst) == -1) {
                    perror("symlink");
                    return EXIT_FAILURE;
                }
                return EXIT_SUCCESS;
            }
        }
        free(real_source_base);
        free(real_target_base);
        
        if (symlink(link_buf, path_dst) == -1) {
            perror("symlink");
            return EXIT_FAILURE;
        }
    } else {
        if (symlink(link_buf, path_dst) == -1) {
            perror("symlink");
            return EXIT_FAILURE;
        }
    }
    
    return EXIT_SUCCESS;
}

int copy_tree(const char* source_path, const char* dest_path, const char* source_base, const char* target_base) {
    struct stat file_stat;
    if (lstat(source_path, &file_stat) == -1) {
        ERR("Failed to get file status");
    }

    if (S_ISREG(file_stat.st_mode)) {
        if (copy_file(source_path, dest_path) == 0) {
            return EXIT_SUCCESS;
        } else {
            ERR("Failed to copy file");
        }
    }

    if (S_ISDIR(file_stat.st_mode)) {
        if (copy_dir(source_path, dest_path, source_base, target_base) == 0) {
            return EXIT_SUCCESS;
        } else {
            ERR("Failed to copy directory");
        }
    }

    if (S_ISLNK(file_stat.st_mode)) {
        if (copy_symlink_smart(source_path, dest_path, source_base, target_base) == 0) {
            return EXIT_SUCCESS;
        } else {
            ERR("Failed to copy symbolic link");
        }
    }

    if (S_ISFIFO(file_stat.st_mode) || S_ISSOCK(file_stat.st_mode) || S_ISBLK(file_stat.st_mode) || S_ISCHR(file_stat.st_mode)) {
        fprintf(stderr, "Skipping special file: %s\n", source_path);
        return EXIT_SUCCESS;
    }

    ERR("Unknown file type");
    return EXIT_FAILURE;
}
int copy_dir(const char* source_path, const char* dest_path, const char* source_base, const char* target_base) {
    struct stat dest_stat;

    if (mkdir(dest_path, 0755) == -1) {
        if (errno == EEXIST) {
            if (lstat(dest_path, &dest_stat) == -1) {
                ERR("Failed to check destination directory");
            }
            if (!S_ISDIR(dest_stat.st_mode)) {
                ERR("Destination exists but is not a directory");
            }
        } else {
            ERR("Failed to create destination directory");
        }
    }

    DIR *dir = opendir(source_path);
    if (!dir) {
        ERR("Failed to open source directory");
    }

    struct dirent *entry;
    char child_source[PATH_MAX];
    char child_dest[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(child_source, PATH_MAX, "%s/%s", source_path, entry->d_name);
        snprintf(child_dest, PATH_MAX, "%s/%s", dest_path, entry->d_name);
        copy_tree(child_source, child_dest, source_base, target_base);
    }

    closedir(dir);
    return EXIT_SUCCESS;
}