#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

static void free_tokens(char **tokens, int cnt) {
    for (int i = 0; i < cnt; i++) free(tokens[i]);
    free(tokens);
}

static char** tokenize(const char *line, int *count) {
    if (!line || !count) return NULL;
    
    char *copy = strdup(line);
    if (!copy) return NULL;
    
    char **tokens = malloc(sizeof(char*) * 16);
    if (!tokens) { free(copy); return NULL; }
    
    *count = 0;
    int cap = 16;
    char *cursor = copy;
    
    while (*cursor) {
        while (*cursor && isspace(*cursor)) cursor++;
        if (!*cursor) break;
        
        char *start = cursor;
        if (*cursor == '"') {
            start = ++cursor;
            while (*cursor && *cursor != '"') cursor++;
            if (*cursor == '\0') {
                fprintf(stderr, "Error: unmatched quote in command\n");
                free_tokens(tokens, *count);
                free(copy);
                return NULL;
            }
        } else {
            while (*cursor && !isspace(*cursor)) {
                if (*cursor == '"') {
                    fprintf(stderr, "Error: unexpected quote in token\n");
                    free_tokens(tokens, *count);
                    free(copy);
                    return NULL;
                }
                cursor++;
            }
        }
        if (*cursor) *cursor++ = '\0';
        
        if (*count >= cap) {
            cap *= 2;
            char **tmp = realloc(tokens, sizeof(char*) * cap);
            if (!tmp) { free_tokens(tokens, *count); free(copy); return NULL; }
            tokens = tmp;
        }
        tokens[(*count)++] = strdup(start);
    }
    free(copy);
    return tokens;
}

static int parse_paths(command_t *cmd, char **tokens, int cnt, int min_args, const char *name) {
    if (cnt < min_args) {
        fprintf(stderr, "Error: '%s' requires source and target path(s)\n", name);
        return -1;
    }
    cmd->source_path = strdup(tokens[1]);
    cmd->target_count = cnt - 2;
    cmd->target_paths = malloc(sizeof(char*) * cmd->target_count);
    for (int i = 0; i < cmd->target_count; i++)
        cmd->target_paths[i] = strdup(tokens[i + 2]);
    return 0;
}

command_t* parse_command(const char *line) {
    if (!line) return NULL;
    
    int cnt = 0;
    char **tokens = tokenize(line, &cnt);
    if (!tokens || cnt == 0) {
        if (tokens) free_tokens(tokens, cnt);
        return NULL;
    }
    
    command_t *cmd = calloc(1, sizeof(command_t));
    if (!cmd) { free_tokens(tokens, cnt); return NULL; }
    
    const char *c = tokens[0];
    
    if (strcmp(c, "add") == 0) {
        cmd->type = CMD_ADD;
        if (parse_paths(cmd, tokens, cnt, 3, "add") < 0) { free(cmd); free_tokens(tokens, cnt); return NULL; }
    }
    else if (strcmp(c, "end") == 0) {
        cmd->type = CMD_END;
        if (parse_paths(cmd, tokens, cnt, 3, "end") < 0) { free(cmd); free_tokens(tokens, cnt); return NULL; }
    }
    else if (strcmp(c, "list") == 0) cmd->type = CMD_LIST;
    else if (strcmp(c, "help") == 0) cmd->type = CMD_HELP;
    else if (strcmp(c, "exit") == 0) cmd->type = CMD_EXIT;
    else if (strcmp(c, "restore") == 0) {
        cmd->type = CMD_RESTORE;
        if (cnt != 3) {
            fprintf(stderr, "Error: 'restore' requires backup and source path\n");
            free(cmd); free_tokens(tokens, cnt); return NULL;
        }
        cmd->target_count = 1;
        cmd->target_paths = malloc(sizeof(char*));
        cmd->target_paths[0] = strdup(tokens[1]);
        cmd->source_path = strdup(tokens[2]);
    }
    else {
        cmd->type = CMD_UNKNOWN;
        fprintf(stderr, "Error: Unknown command '%s'\n", c);
    }
    
    free_tokens(tokens, cnt);
    return cmd;
}

void free_command(command_t *cmd) {
    if (!cmd) return;
    free(cmd->source_path);
    for (int i = 0; i < cmd->target_count; i++) free(cmd->target_paths[i]);
    free(cmd->target_paths);
    free(cmd);
}
