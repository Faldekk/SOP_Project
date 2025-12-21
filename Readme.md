# sop-backup

Real-time backup system using inotify for Linux.

## Build & Run

```bash
make
./sop-backup
```

## Commands

| Command | Description |
|---------|-------------|
| `add <src> <dst>` | Start backup from source to destination |
| `end <src> <dst>` | Stop backup |
| `list` | Show active backups |
| `restore <backup> <target>` | Restore backup |
| `help` | Show commands |
| `exit` | Exit program |

## Features

- Live file monitoring (inotify)
- Recursive directory backup
- Symlink handling
- Multiple backup targets
- Signal handling (SIGINT, SIGTERM)

## Architecture

```
main.c ──────► parser.c ──────► backup_manager.c
   │                                   │
   │                                   ▼
   │                            monitor.c (fork)
   │                                   │
   ▼                                   ▼
signals.c                         backup.c
                                       │
                                       ▼
                                   restore.c
```

### Modules

| File | Description |
|------|-------------|
| `main.c` | Main loop, command handling, user interface |
| `parser.c` | Parses user input into command structures |
| `backup_manager.c` | Manages active backups, spawns worker processes |
| `monitor.c` | Inotify watcher, detects file changes in real-time |
| `backup.c` | File/directory copy operations (bulk read/write) |
| `restore.c` | Restores backup to original location |
| `signals.c` | SIGINT/SIGTERM handlers for graceful shutdown |
| `utils.c` | Helper functions, error macros |

### How it works

1. **User runs `add src dst`** → `parser.c` parses command
2. **`backup_manager.c`** creates initial backup using `backup.c`
3. **Fork** → child process runs `monitor.c`
4. **`monitor.c`** sets up inotify watches on all directories
5. **File change detected** → `backup.c` syncs changes to backup
6. **User runs `end`** → worker process is killed (SIGTERM)

## Project

SOP1 @ Warsaw University of Technology

https://sop.mini.pw.edu.pl/en/sop1/project/

