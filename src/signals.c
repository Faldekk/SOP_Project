// clang-format off
#define _GNU_SOURCE
#include "signals.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "backup_manager.h"

// handlin signals like ctrl+c
static void signal_handler(int signo)
{
    (void)signo;
    should_exit = 1;
    write(STDERR_FILENO, "\nShutting down...\n", 19);
}

// settin up all the signal handlers
void setup_signal_handlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
    }

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction SIGTERM");
    }

    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        perror("sigaction SIGHUP");
    }

    if (sigaction(SIGQUIT, &sa, NULL) == -1)
    {
        perror("sigaction SIGQUIT");
    }

    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
    {
        perror("sigaction SIGPIPE");
    }
}

// cleanin up before exit
void cleanup_on_exit(void *manager)
{
    if (manager)
    {
        backup_manager_t *mgr = (backup_manager_t *)manager;
        kill_all_workers(mgr);
    }
    write(STDOUT_FILENO, "Cleanup completed. Exiting.\n", 29);
}
