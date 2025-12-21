#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "utils.h"

static char** tokenize(const char *line, int *count) {
    if (!line || !count) return NULL;
    
    char *line_copy = strdup(line);
    if (!line_copy) return NULL;
    
    int capacity = 10;
    char **tokens = malloc(sizeof(char*) * capacity);
    if (!tokens) {
        free(line_copy);
        return NULL;
    }
    
    *count = 0;
    char *cursor = line_copy;
    
    while (*cursor) {
        while (*cursor && isspace(*cursor)) cursor++;
        if (!*cursor) break;
        
        char *token_start = cursor;
        
        if (*cursor == '"') {
            cursor++;
            token_start = cursor;
            while (*cursor && *cursor != '"') cursor++;
            if (*cursor == '"') {
                *cursor = '\0';
                cursor++;
            }
        } else {
            while (*cursor && !isspace(*cursor)) cursor++;
            if (*cursor) {
                *cursor = '\0';
                cursor++;
            }
        }
        
        if (*count >= capacity) {
            capacity *= 2;
            char **new_tokens = realloc(tokens, sizeof(char*) * capacity);
            if (!new_tokens) {
                for (int i = 0; i < *count; i++) free(tokens[i]);
                free(tokens);
                free(line_copy);
                return NULL;
            }
            tokens = new_tokens;
        }
        
        tokens[*count] = strdup(token_start);
        if (!tokens[*count]) {
            for (int i = 0; i < *count; i++) free(tokens[i]);
            free(tokens);
            free(line_copy);
            return NULL;
        }
        (*count)++;
    }
    
    free(line_copy);
    return tokens;
}

command_t* parse_command(const char *line) {
    if (!line) return NULL;
    
    int cnt = 0;
    char **tokens = tokenize(line, &cnt);
    
    if (!tokens || cnt == 0) {
        if (tokens) {
            for (int i = 0; i < cnt; i++) free(tokens[i]);
            free(tokens);
        }
        return NULL;
    }
    
    command_t *cmd = calloc(1, sizeof(command_t));
    if (!cmd) {
        for (int i = 0; i < cnt; i++) free(tokens[i]);
        free(tokens);
        return NULL;
    }
    
    if (strcmp(tokens[0], "add") == 0) {
        cmd->type = CMD_ADD;
        if (cnt < 3) {
            fprintf(stderr, "Error: 'add' requires source and at least one target path\n");
            free(cmd);
            for (int i = 0; i < cnt; i++) free(tokens[i]);
            free(tokens);
            return NULL;
        }
        cmd->source_path = strdup(tokens[1]);
        cmd->target_count = cnt - 2;
        cmd->target_paths = malloc(sizeof(char*) * cmd->target_count);
        for (int i = 0; i < cmd->target_count; i++) {
            cmd->target_paths[i] = strdup(tokens[i + 2]);
        }
    }
    else if (strcmp(tokens[0], "end") == 0) {
        cmd->type = CMD_END;
        if (cnt < 3) {
            fprintf(stderr, "Error: 'end' requires source and at least one target path\n");
            free(cmd);
            for (int i = 0; i < cnt; i++) free(tokens[i]);
            free(tokens);
            return NULL;
        }
        cmd->source_path = strdup(tokens[1]);
        cmd->target_count = cnt - 2;
        cmd->target_paths = malloc(sizeof(char*) * cmd->target_count);
        for (int i = 0; i < cmd->target_count; i++) {
            cmd->target_paths[i] = strdup(tokens[i + 2]);
        }
    }
    else if (strcmp(tokens[0], "list") == 0) {
        cmd->type = CMD_LIST;
    }
    else if (strcmp(tokens[0], "restore") == 0) {
        cmd->type = CMD_RESTORE;
        if (cnt != 3) {
            fprintf(stderr, "Error: 'restore' requires exactly backup and source path\n");
            free(cmd);
            for (int i = 0; i < cnt; i++) free(tokens[i]);
            free(tokens);
            return NULL;
        }
        cmd->target_count = 1;
        cmd->target_paths = malloc(sizeof(char*));
        cmd->target_paths[0] = strdup(tokens[1]);
        cmd->source_path = strdup(tokens[2]);
    }
    else if (strcmp(tokens[0], "exit") == 0) {
        cmd->type = CMD_EXIT;
    }
    else if(strcmp(tokens[0],"help")==0){
        cmd->type = CMD_HELP;
    }
    else {
        cmd->type = CMD_UNKNOWN;
        fprintf(stderr, "Error: Unknown command '%s'\n", tokens[0]);
    }
    
    for (int i = 0; i < cnt; i++) free(tokens[i]);
    free(tokens);
    
    return cmd;
}

void free_command(command_t *cmd) {
    if (!cmd) return;
    
    free(cmd->source_path);
    
    if (cmd->target_paths) {
        for (int i = 0; i < cmd->target_count; i++) {
            free(cmd->target_paths[i]);
        }
        free(cmd->target_paths);
    }
    
    free(cmd);
}
