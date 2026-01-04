CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -O2 -g -Iinclude
LDFLAGS=-pthread

SRC = src/main.c src/traversal.c src/hash_table.c
OUT = du-sync

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
