sop-backup - live backup system with inotify monitoring

Project info at: https://sop.mini.pw.edu.pl/en/sop1/project/

How to build and run

Interactive only. Compile and start the prompt:
```bash
make
./sop-backup
```

Interactive mode - type commands at the prompt:
```
> add "/home/user/docs" "/backup/docs"
> list
> end "/home/user/docs" "/backup/docs"
> exit
```

Commands explained

add <source> <target> [more targets...]
Creates initial backup by copying everything from source to target(s). Target directories must be empty or will be created. After that it forks a worker process that watches for changes using inotify and automatically syncs them. You can have multiple targets for one source.

end <source> <target> [more targets...]
Stops the backup worker for given source-target pair. Your backup stays where it is, just stops monitoring. Sends SIGTERM to the worker, waits 500ms, then sends SIGKILL if it's still alive.

list
Shows all active backups with their PIDs. Nothing fancy.

restore <source> <target>
Restores backup from target back to source. Deletes files that aren't in the backup anymore and copies files that changed (compares by mtime+size hash). This one blocks until it's done.

exit
Kills all workers and exits cleanly. You can also just Ctrl+C or send SIGTERM/SIGINT/SIGHUP/SIGQUIT.

What's under the hood

inotify monitoring - Uses Linux inotify API to watch for file events. Monitors IN_CREATE, IN_MODIFY, IN_DELETE, IN_MOVED_FROM, IN_MOVED_TO, IN_CLOSE_WRITE. Workers poll every 100ms.

Debug mode - Set DEBUG_INOTIFY=1 environment variable to see what inotify is doing. Shows all events including IN_OPEN and IN_ACCESS which are usually ignored.

Race condition handling - If a file disappears while being copied (like Vim's .swp files), it just ignores it instead of crashing. Checks for ENOENT errors.

Signal handling - No SA_RESTART flag on signal handlers, so signals can interrupt blocking calls like getline(). Responds to SIGINT/SIGTERM/SIGHUP/SIGQUIT by setting should_exit flag and cleaning up.

Smart symlinks - Rewrites absolute symlinks that point inside source directory to point inside target instead. Relative symlinks are just copied as-is.

Process management - Parent process manages child workers. When killing workers, tries SIGTERM first, waits 500ms, then uses SIGKILL if needed.

Important stuff to know

Don't backup a directory into itself (like /home/user into /home/user/backup). It'll cause recursive madness and the program rejects it anyway.

Can't have duplicate backups with same source and target. Program checks and rejects duplicates.

Symlinks are preserved but absolute paths get rewritten if they point inside the source directory.

Changes sync automatically but there's a 100ms polling delay since inotify is non-blocking.

File hash is just mtime+size, not a real cryptographic hash. Good enough for detecting changes though.

Architecture

Main process creates backup_manager_t, sets up signal handlers, runs interactive loop or single command from CLI.

When you add a backup, it forks a child process. Child calls start_backup_worker() which sets up inotify, adds recursive watches, and loops reading events until should_exit is set.

Parent maintains linked list of backup_entry_t with source path, target path, worker PID.

When you end a backup, parent kills the worker process and removes entry from list.

Code is in C17, uses _GNU_SOURCE, compiles with sanitizers (address, undefined, leak).

------------------------ Functions descriptions -----------------------------

API Documentation - sop-backup functions

backup.c - file and directory copying operations

bulk_read(fd, buf, count)
Low-level read function that handles signal interruptions properly. Takes file descriptor, buffer, and byte count. Returns number of bytes read (ssize_t) or -1 on error. Uses TEMP_FAILURE_RETRY to restart interrupted system calls.

bulk_write(fd, buf, count)
Low-level write function with signal interrupt handling. Parameters: fd (file descriptor), buf (data buffer), count (bytes to write). Returns number of bytes written or -1 on error.

copy_file(src, dst)
Copies a single file from source to destination using 64KB buffers. Preserves permissions with chmod. Handles race conditions - returns -1 without calling ERR() when errno is ENOENT (file disappeared during copy, like Vim swap files). Parameters: src (source path), dst (destination path). Returns 0 on success, -1 on error.

copy_symlink(src, dst)
Copies a symbolic link to destination path. Reads the link target with readlink() and creates new symlink at destination. Parameters: src (symlink path), dst (destination path). Returns EXIT_SUCCESS or calls ERR() on failure.

copy_symlink_smart(src, dst, source_base, target_base)
Smart symlink copying that handles absolute paths. If a symlink points to an absolute path inside source_base, it rewrites it to point inside target_base instead. Uses readlink() to get target, strncmp() to check if target starts with source_base, then rewrites the path. Parameters: src (source symlink), dst (destination), source_base (source directory), target_base (target directory). Returns 0 on success, -1 on error.

copy_tree(src_dir, dst_dir)
Recursively copies directory contents. Detects file type (regular file/directory/symlink) using lstat() and calls appropriate copy function. Parameters: src_dir (source directory), dst_dir (destination directory). Returns EXIT_SUCCESS or EXIT_FAILURE.

copy_dir(src, dst)
Copies a directory and all its contents recursively. Parameters: src (source directory), dst (destination directory). Returns EXIT_SUCCESS.

backup_manager.c - backup management

create_backup_manager()
Creates manager structure with initialized inotify. Returns pointer to backup_manager_t or NULL on error. Initializes linked list head to NULL and creates inotify fd with IN_NONBLOCK and IN_CLOEXEC flags.

destroy_backup_manager(mgr)
Frees resources and kills all worker processes. Calls kill_all_workers(), then iterates through linked list freeing all backup entries and their paths. Closes inotify fd. Parameters: mgr (manager). Returns void.

add_backup(mgr, source, target)
Adds new backup to list and forks worker process to monitor changes. Checks for duplicates first, creates backup_entry_t, then forks. Child process calls start_backup_worker() and exits. Parent process saves PID and returns. Parameters: mgr, source, target. Returns 0 on success, -1 on error.

remove_backup(mgr, source, target)
Removes backup from list, kills worker process, and removes inotify watch. Sends SIGTERM to worker and waits for it. Unlinks entry from list and frees memory. Parameters: mgr, source, target. Returns 0 on success, -1 if backup not found.

list_backups(mgr)
Prints list of active backups with their worker PIDs. Just iterates through linked list and prints source -> target pairs with PID. Parameters: mgr. Returns void.

kill_all_workers(mgr)
Terminates all worker processes with grace period. Sends SIGTERM, waits 500ms, then sends SIGKILL if process still alive. Implementation: iterates through all backup_entry, kill(SIGTERM), usleep(200ms), waitpid(WNOHANG), usleep(300ms more), SIGKILL if ESRCH. Parameters: mgr. Returns void.

monitor.c - change monitoring and initial backups

create_initial_backup(source, target)
Checks conditions (source exists, target empty, not backing up into itself) and runs copy_tree. Uses realpath() to resolve paths and check for recursive backup. Creates target directory if missing. Parameters: source (source path), target (target path). Returns 0 on success, -1 on error.

is_directory_empty(path)
Checks if directory is empty (no files except . and ..). Opens directory with opendir(), reads entries with readdir(), counts non-dot entries. Parameters: path (directory path). Returns 1 if empty, 0 if contains files.

add_watch_recursive(inotify_fd, path)
Adds inotify watch recursively on directory and all subdirectories. Opens directory, reads entries, adds watch for current directory, then recurses into subdirectories. inotify flags: IN_CREATE, IN_MODIFY, IN_DELETE, IN_MOVED_FROM, IN_MOVED_TO, IN_CLOSE_WRITE (plus IN_OPEN and IN_ACCESS in debug mode). Parameters: inotify_fd (inotify descriptor), path (directory to monitor). Returns 0 on success, -1 on error.

handle_inotify_event(event, source, target, inotify_fd)
Handles inotify events and syncs changes to backup. Event types:
  - IN_CREATE: mkdir + add_watch_recursive for directories, copy_file for files
  - IN_MODIFY/IN_CLOSE_WRITE: copy_file
  - IN_DELETE: unlink or rmdir
  - IN_MOVED_FROM: unlink or rmdir

Debug mode: when DEBUG_INOTIFY=1 env var is set, logs all events including IN_OPEN and IN_ACCESS which are normally ignored.

Race condition handling: checks for errno == ENOENT and ignores temporary files that disappear during processing (like Vim swap files).

Parameters: event, source, target, inotify_fd. Returns void.

start_backup_worker(source, target)
Main worker process loop (runs in child after fork):
1. Calls setup_signal_handlers() for SIGINT/SIGTERM/SIGHUP/SIGQUIT handling
2. Initializes inotify with inotify_init1(IN_NONBLOCK | IN_CLOEXEC)
3. Adds recursive watches with add_watch_recursive()
4. Loop while(!should_exit): reads inotify events (non-blocking), handles each event, usleep(100ms)
5. Checks if source still exists with stat()

Parameters: source, target. Returns void (calls exit(EXIT_SUCCESS) at end).

parser.c - command parsing

command_type_t (enum)
Command types: CMD_ADD, CMD_END, CMD_LIST, CMD_RESTORE, CMD_EXIT, CMD_UNKNOWN

command_t (structure)
Fields: type, source_path, target_paths[], target_count

tokenize(line, count)
Splits line into tokens, handles spaces and quotes. Uses state machine to track whether we're inside quotes. Parameters: line, count (pointer to token count). Returns array of token pointers or NULL.

parse_command(line)
Parses line into command_t structure. Handles commands: add, end, list, restore, exit. Allocates memory for command structure and paths. Parameters: line. Returns pointer to command_t or NULL.

free_command(cmd)
Frees memory for command_t structure including all path strings. Parameters: cmd. Returns void.

restore.c - backup restoration

calculate_file_hash(filepath)
Calculates simple file hash using stat - mtime (modification time) and size. Not a real cryptographic hash, just good enough for change detection. Implementation: stat(), snprintf("%ld-%ld", st_mtime, st_size), strdup(). Parameters: filepath. Returns pointer to "mtime-size" string or NULL on error.

compare_and_copy_if_different(src, dst)
Compares file hashes (mtime+size) and copies src to dst if different. Implementation: calculate_file_hash() for both files, strcmp(), copy_file() if different. Parameters: src (source file from backup), dst (destination file to restore). Returns 0 on success, -1 on error.

delete_files_not_in_backup(source, target)
Deletes files from source if they don't exist in target (backup). Recursively walks through source directory and checks if each entry exists in target. Parameters: source (to clean), target (backup). Returns void.

restore_backup(source, target)
Main restore function:
1. Deletes files not in backup
2. Walks through backup directory
3. For each file: compare_and_copy_if_different
4. For directories: mkdir and recurse

Parameters: source (to restore), target (backup). Returns 0 on success, -1 on error.

signals.c - signal handling

signal_handler(signo)
Signal handler that sets should_exit = 1 and prints message. Handles: SIGINT, SIGTERM, SIGHUP, SIGQUIT. Uses write() instead of printf() because it's signal-safe. Switch statement with different messages for each signal type. Parameters: signo. Returns void.

setup_signal_handlers()
Registers signal handlers:
  - SIGINT: signal_handler (no SA_RESTART - allows interrupting blocking calls like getline)
  - SIGTERM: signal_handler (no SA_RESTART)
  - SIGHUP: signal_handler (no SA_RESTART)
  - SIGQUIT: signal_handler (no SA_RESTART)
  - SIGPIPE: SIG_IGN

Important: no SA_RESTART flag allows signals to interrupt blocking system calls (getline, read). Returns void.

cleanup_on_exit(manager)
Casts manager to backup_manager_t pointer and calls kill_all_workers. Parameters: manager (void pointer). Returns void.

main.c - main program loop

print_help()
Displays available commands to stdout.

main(argc, argv)
Interactive-only main loop (argc ignored beyond argv[0]). Runs getline() loop with ">" prompt.

Initialization: creates backup_manager, sets up signal handlers.

Commands:
  - add: create_initial_backup + add_backup (forks worker)
  - end: remove_backup (kills worker)
  - list: list_backups
  - restore: restore_backup (blocks until done)
  - exit: cleanup_on_exit + exits program

Signal handling: getline() interrupted by SIGINT/SIGTERM (no SA_RESTART), checks should_exit || feof().

Returns EXIT_SUCCESS or EXIT_FAILURE.

utils.c - helper functions

ERR (macro)
Error handling macro - calls perror, prints file and line number with fprintf, then exits. Used throughout code for fatal errors.

usage()
Prints usage information to stderr and exits.

print_help()
Prints available commands to stdout.


