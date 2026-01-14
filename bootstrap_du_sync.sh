# bootstrap_du_sync.sh
set -euo pipefail

mkdir -p include src tests .vscode

cat > Makefile <<'EOF'
CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnull-dereference -Wformat=2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

BIN := du-sync
SRC := src/main.c src/du_sync.c src/inode_set.c src/path_util.c src/strvec.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean test format

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f $(BIN) $(OBJ)

test: all
	./tests/run.sh

format:
	@echo "No formatter configured. (Optional) Consider clang-format."

	EOF

	cat > README.md <<'EOF'
# du-sync (Sequential Phase)

A small C program inspired by `du -sb` that **sums the sizes of regular files** (bytes)
under each input path, **avoids double counting hard links** via `(st_dev, st_ino)` tracking,
and **continues on errors** (permission issues etc.) while printing warnings to `stderr`.

## Build
```sh
make
