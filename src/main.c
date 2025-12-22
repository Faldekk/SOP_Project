#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "backup.h"
#include "backup_manager.h"
#include "monitor.h"
#include "signals.h"
#include "parser.h"
#include "restore.h"

volatile sig_atomic_t should_exit = 0;

void print_help() {
    fprintf(stdout, "Available commands:\n");
    fprintf(stdout, "  add <source> <target> [<target> ...] - Start backup\n");
    fprintf(stdout, "  end <source> <target> [<target> ...] - Stop backup\n");
    fprintf(stdout, "  help - prints out the functions usage\n");
    fprintf(stdout, "  list - Show active backups\n");
    fprintf(stdout, "  restore <backup> <source> - Restore backup to source\n");
    fprintf(stdout, "  exit - Exit program\n");
}

int main(int argc, char *argv[]) { 
    const char *home = getenv("HOME");
    if (home) {
        if (chdir(home) != 0) {
            perror("chdir to HOME failed");
        }
    }
    
    setup_signal_handlers();
    
    backup_manager_t *manager = create_backup_manager();
    if (!manager) {
        fprintf(stderr, "Failed to create backup manager\n");
        return EXIT_FAILURE;
    }
    
    fprintf(stdout, "Backup system started.\n");
    print_help();
    
    char *line = NULL;
    size_t len = 0;
    
    while (!should_exit) {
        printf("> ");
        fflush(stdout);
        
        ssize_t nread = getline(&line, &len, stdin);
        if (nread == -1) {
            if (feof(stdin) || should_exit) {
                printf("\n");
                break;
            }
            continue;
        }
        if (line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
        }
        if (line[0] == '\0') continue;
        
        command_t *cmd = parse_command(line);
        if (!cmd) continue;
        
        switch (cmd->type) {
            case CMD_ADD:
                printf("Adding backup: %s\n", cmd->source_path);
                for (int i = 0; i < cmd->target_count; i++) {
                    fprintf(stdout,"  Target: %s\n", cmd->target_paths[i]);
                    
                    if (create_initial_backup(cmd->source_path, cmd->target_paths[i]) != 0) {
                        fprintf(stderr, "Failed to create backup for %s -> %s\n", 
                                cmd->source_path, cmd->target_paths[i]);
                        continue;
                    }
                    
                    if (add_backup(manager, cmd->source_path, cmd->target_paths[i]) == 0) {
                        fprintf(stdout, "Backup added successfully: %s -> %s\n", 
                               cmd->source_path, cmd->target_paths[i]);
                    }
                }
                break;
                
            case CMD_END:
                fprintf(stdout, "Ending backup: %s\n", cmd->source_path);
                for (int i = 0; i < cmd->target_count; i++) {
                    if (remove_backup(manager, cmd->source_path, cmd->target_paths[i]) == 0) {
                        fprintf(stdout, "Backup ended: %s -> %s\n", cmd->source_path, cmd->target_paths[i]);
                    }
                }
                break;
                
            case CMD_LIST:
                list_backups(manager);
                break;
            case CMD_HELP:
                print_help();
                break;
            case CMD_RESTORE:
                if (cmd->target_count > 0) {
                    fprintf(stdout, "Restoring backup: %s -> %s\n", cmd->target_paths[0], cmd->source_path);
                    if (restore_backup(cmd->source_path, cmd->target_paths[0]) == 0) {
                        fprintf(stdout, "Restore completed successfully\n");
                    } else {
                        fprintf(stderr, "Restore failed\n");
                    }
                } else {
                    fprintf(stderr, "Error: restore requires backup and source path\n");
                }
                break;
                
            case CMD_EXIT:
                fprintf(stdout, "Exiting...\n");
                should_exit = 1;
                break;
                
            case CMD_UNKNOWN:
                fprintf(stderr, "Unknown command. Type 'help' for available commands.\n");
                break;
            default:
                break;
        }
        
        free_command(cmd);
    }
    
    free(line);
    
    cleanup_on_exit(manager);
    destroy_backup_manager(manager);
    unlink("/tmp/sop-backup.state");
    return EXIT_SUCCESS;
}
