#ifndef INODE_SET_H
#define INODE_SET_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct InodeKey {
    dev_t dev;
    ino_t ino;
} InodeKey;

typedef struct InodeSet InodeSet;

InodeSet *inode_set_create(void);
void inode_set_destroy(InodeSet *set);

/*
 * Inserts key if absent.
 * Returns:
 *   true  if inserted (was not present),
 *   false if already present.
 * On OOM, sets *oom = true (if oom != NULL) and returns false.
 */
bool inode_set_insert(InodeSet *set, InodeKey key, bool *oom);

#endif /* INODE_SET_H */
