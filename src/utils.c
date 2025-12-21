#include <errno.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage()
{
    fprintf(stderr, "USAGE: sop-backup - Interactive backup management system\\n");
    fprintf(stderr, "Commands: add, end, list, restore, help, exit\\n");
    exit(EXIT_FAILURE);
}