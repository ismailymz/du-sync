# du-sync (Sequential Phase)

`du-sync` is a C implementation inspired by `du -sb`.  
It traverses a directory tree, sums the sizes of regular files (bytes), avoids double counting hard links via inode tracking, and continues gracefully on permission errors.

## Features
- Recursive directory traversal using Linux APIs (`opendir`, `readdir`, `lstat`)
- Counts **regular files** (`S_ISREG`) in **bytes**
- Hard link protection using `(st_dev, st_ino)` inode tracking
- Robust error handling (prints warnings to `stderr`, continues traversal)
- Automated tests comparing output to `du -sb`

new branch added.

## Project Structure
