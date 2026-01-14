CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnull-dereference -Wformat=2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

BIN := du-sync

# These must match the actual .c files in src/
SRC := src/main.c \
       src/du_sync.c \
       src/inode_set.c \
       src/path_util.c \
       src/strvec.c

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