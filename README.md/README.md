du-sync (Sequential || Parallel) - Portfolio Project

du-sync => a C program inspired by `du -sb` it traverses a dir tree, sums the sizes of "regular files" (bytes), avoids double counting hardlinks via inode tracking, continues on permission errors. It also supports parallel traverseal using threads (`-j`) and has a thread-debug mode (`--debug-threads`) for demos.

Features

Core Behavior
-Recursive directional traversal : using Linux APIs: `opendir`, `readdir`, `closedir`, `lstat`
-Counts only **regular files** (`S_ISREG`) using `st_size` (bytes)
Robust error handling:
  - prints warnings to `stderr`
  - continues traversal (does not crash) on permission/stat/open errors
- Output format:
    <bytes>\t<path>


    CLI + pipeline support
  
- Supports normal CLI paths: `./du-sync PATH...`
- Supports stdin input using `-`:
- newline-delimited input by default
- NUL-delimited input with `-0` (works well with `find -print0`)
- Designed for pipelines: writes results to **stdout**, logs/warnings/debug to **stderr**
- 

Parallel Mode (threads)


- `-j N` / `--jobs N`: traverses directories using a worker thread pool
- Synchronization with mutexes/condition variables for safe access to shared structures
- `--debug-threads`: prints worker thread activity and thread IDs to `stderr`
