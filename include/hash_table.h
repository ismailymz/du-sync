#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <sys/types.h>  // dev_t, ino_t
#include <stddef.h>     // size_t

typedef struct InodeKey {
    dev_t dev;
    ino_t ino;
} InodeKey;

typedef struct InodeHashSet InodeHashSet;

// create/destroy
InodeHashSet* inode_set_create(size_t bucket_count);
void inode_set_destroy(InodeHashSet *set);

// returns 1 if already present, 0 if inserted now, -1 on error (OOM)
int inode_set_check_or_add(InodeHashSet *set, dev_t dev, ino_t ino);

#endif
