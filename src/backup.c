// clang-format off
#define _GNU_SOURCE
#include "backup.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define FILE_BUF_LEN 65536

// bulk read and write from lecture, dont touch this
ssize_t bulk_read(int fd, char* buf, size_t count)
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

// same as above but for writin
ssize_t bulk_write(int fd, char* buf, size_t count)
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

// copyin file from src to dst, also preservs the time
int copy_file(const char* source_path, const char* dest_path)
{
    struct stat source_stat;
    if (stat(source_path, &source_stat) == -1)
    {
        if (errno == ENOENT)
        {
            return -1;
        }
        ERR("Failed to get source file info");
    }

    const int source_fd = open(source_path, O_RDONLY);
    if (source_fd == -1)
    {
        if (errno == ENOENT)
        {
            return -1;
        }
        ERR("Failed to open source file");
    }

    const int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, source_stat.st_mode & 0777);
    if (dest_fd == -1)
    {
        close(source_fd);
        ERR("Failed to create destination file");
    }

    char buffer[FILE_BUF_LEN];
    for (;;)
    {
        const ssize_t bytes_read = bulk_read(source_fd, buffer, FILE_BUF_LEN);
        if (bytes_read == -1)
        {
            close(source_fd);
            close(dest_fd);
            ERR("Failed to read from source file");
        }
        if (bytes_read == 0)
        {
            break;
        }
        if (bulk_write(dest_fd, buffer, bytes_read) == -1)
        {
            close(source_fd);
            close(dest_fd);
            ERR("Failed to write to destination file");
        }
    }

    close(source_fd);
    close(dest_fd);

    // settin file times to match original
    struct timeval times[2] = {{source_stat.st_atime, 0}, {source_stat.st_mtime, 0}};
    utimes(dest_path, times);

    return EXIT_SUCCESS;
}

// copyin symlink, this was annoyin to make work
int copy_symlink(const char* path_src, const char* path_dst, const char* source_base, const char* target_base)
{
    char link_buf[PATH_MAX];
    ssize_t len = readlink(path_src, link_buf, PATH_MAX - 1);
    if (len == -1)
    {
        perror("readlink");
        return EXIT_FAILURE;
    }
    link_buf[len] = '\0';

    // if link points to file in source, redirect to target
    if (link_buf[0] == '/')
    {
        char* real_src = realpath(source_base, NULL);
        char* real_tgt = realpath(target_base, NULL);

        if (real_src && real_tgt && strncmp(link_buf, real_src, strlen(real_src)) == 0)
        {
            char new_path[PATH_MAX];
            snprintf(new_path, PATH_MAX, "%s%s", real_tgt, link_buf + strlen(real_src));
            free(real_src);
            free(real_tgt);
            if (symlink(new_path, path_dst) == -1)
            {
                perror("symlink");
                return EXIT_FAILURE;
            }
            return EXIT_SUCCESS;
        }
        free(real_src);
        free(real_tgt);
    }

    if (symlink(link_buf, path_dst) == -1)
    {
        perror("symlink");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// checcs file type and calls correct copy function
int copy_tree(const char* source_path, const char* dest_path, const char* source_base, const char* target_base)
{
    struct stat st;
    if (lstat(source_path, &st) == -1)
    {
        ERR("lstat");
    }

    if (S_ISREG(st.st_mode))
        return copy_file(source_path, dest_path);

    if (S_ISDIR(st.st_mode))
        return copy_dir(source_path, dest_path, source_base, target_base);

    if (S_ISLNK(st.st_mode))
        return copy_symlink(source_path, dest_path, source_base, target_base);

    // skipin special files like fifos and sockets
    return EXIT_SUCCESS;
}

// copyin whole directory recursivly
int copy_dir(const char* source_path, const char* dest_path, const char* source_base, const char* target_base)
{
    struct stat src_stat;
    if (lstat(source_path, &src_stat) == -1)
    {
        ERR("lstat");
    }

    mode_t old_umask = umask(0);
    int ret = mkdir(dest_path, src_stat.st_mode & 0777);
    umask(old_umask);

    if (ret == -1 && errno != EEXIST)
    {
        ERR("mkdir");
    }

    DIR* dir = opendir(source_path);
    if (!dir)
    {
        ERR("opendir");
    }

    struct dirent* entry;
    char child_src[PATH_MAX], child_dst[PATH_MAX];

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(child_src, PATH_MAX, "%s/%s", source_path, entry->d_name);
        snprintf(child_dst, PATH_MAX, "%s/%s", dest_path, entry->d_name);
        copy_tree(child_src, child_dst, source_base, target_base);
    }

    closedir(dir);
    return EXIT_SUCCESS;
}
