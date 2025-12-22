#ifndef BACKUP_H
#define BACKUP_H

ssize_t bulk_read(int fd, char *buf, size_t count);
ssize_t bulk_write(int fd, char *buf, size_t count);

int copy_file(const char *src, const char *dst);
int copy_symlink(const char *src, const char *dst, const char *source_base, const char *target_base);
int copy_tree(const char *src_dir, const char *dst_dir, const char *source_base, const char *target_base);
int copy_dir(const char *src, const char *dst, const char *source_base, const char *target_base);

#endif
