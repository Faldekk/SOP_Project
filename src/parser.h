#ifndef PARSER_H
#define PARSER_H

typedef enum
{
    CMD_ADD,
    CMD_END,
    CMD_LIST,
    CMD_HELP,
    CMD_RESTORE,
    CMD_EXIT,
    CMD_UNKNOWN
} command_type_t;

typedef struct
{
    command_type_t type;
    char *source_path;
    char **target_paths;
    int target_count;
} command_t;

command_t *parse_command(const char *line);
void free_command(command_t *cmd);

#endif
