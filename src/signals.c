#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "signals.h"
#include "backup_manager.h"

static void signal_handler(int signo) {
    should_exit = 1;
    const char *msg = NULL;
    
    switch(signo) {
        case SIGINT:
            msg = "\nReceived SIGINT, shutting down...\n";
            break;
        case SIGTERM:
            msg = "\nReceived SIGTERM, shutting down...\n";
            break;
        case SIGHUP:
            msg = "\nReceived SIGHUP, shutting down...\n";
            break;
        case SIGQUIT:
            msg = "\nReceived SIGQUIT, shutting down...\n";
            break;
        default:
            msg = "\nReceived signal, shutting down...\n";
            break;
    }
    
    if (msg) {
        write(STDERR_FILENO, msg, strlen(msg));
    }
}

void setup_signal_handlers() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
    }
    
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction SIGHUP");
    }
    
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("sigaction SIGQUIT");
    }
    
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction SIGPIPE");
    }
}

void cleanup_on_exit(void *manager) {
    if (manager) {
        backup_manager_t *mgr = (backup_manager_t *)manager;
        kill_all_workers(mgr);
    }
    fprintf(stdout, "Cleanup completed\n");
}
