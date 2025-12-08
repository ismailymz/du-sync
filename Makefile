# Variables
CC = gcc
CFLAGS = -Wall -Wextra -pthread

# The 'all' target is required for the exam evaluation
all: build

build:
	$(CC) $(CFLAGS) -o du-sync main.c hash_table.c

# Required target to remove generated files
clean:
	rm -f du-sync

# Required target to execute your tests
test: build
	./run_tests.sh