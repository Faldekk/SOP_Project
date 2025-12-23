// clang-format off
#ifndef RESTORE_H
#define RESTORE_H

#include "backup.h"

int restore_backup(const char *source, const char *target);
int compare_and_copy_if_different(const char *src, const char *dst);
void delete_files_not_in_backup(const char *source, const char *target);

#endif
