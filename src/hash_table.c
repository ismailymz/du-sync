#include "hash_table.h"
#include <stdlib.h>
#include <stdint.h>

typedef struct Node {
    InodeKey key;
    struct Node *next;
} Node;

struct InodeHashSet {
    size_t bucket_count;
    Node **buckets;
};

// basit ama iyi karışan hash (dev+ino)
static size_t hash_key(dev_t dev, ino_t ino, size_t bucket_count) {
    // 64-bit karıştırma (mix)
    uint64_t x = (uint64_t)(uintmax_t)dev;
    uint64_t y = (uint64_t)(uintmax_t)ino;

    uint64_t h = x;
    h ^= y + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    // bucket index
    return (size_t)(h % bucket_count);
}

InodeHashSet* inode_set_create(size_t bucket_count) {
    if (bucket_count == 0) bucket_count = 4096;

    InodeHashSet *set = (InodeHashSet*)malloc(sizeof(InodeHashSet));
    if (!set) return NULL;

    set->bucket_count = bucket_count;
    set->buckets = (Node**)calloc(bucket_count, sizeof(Node*));
    if (!set->buckets) {
        free(set);
        return NULL;
    }
    return set;
}

void inode_set_destroy(InodeHashSet *set) {
    if (!set) return;

    for (size_t i = 0; i < set->bucket_count; i++) {
        Node *cur = set->buckets[i];
        while (cur) {
            Node *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }

    free(set->buckets);
    free(set);
}

// 1: zaten vardı, 0: şimdi eklendi, -1: malloc fail
int inode_set_check_or_add(InodeHashSet *set, dev_t dev, ino_t ino) {
    if (!set) return -1;

    size_t idx = hash_key(dev, ino, set->bucket_count);
    Node *cur = set->buckets[idx];

    while (cur) {
        if (cur->key.dev == dev && cur->key.ino == ino) {
            return 1; // already present
        }
        cur = cur->next;
    }

    Node *n = (Node*)malloc(sizeof(Node));
    if (!n) return -1;

    n->key.dev = dev;
    n->key.ino = ino;
    n->next = set->buckets[idx];
    set->buckets[idx] = n;

    return 0; // inserted
}
