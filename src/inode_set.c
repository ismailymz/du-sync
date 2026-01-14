#include "inode_set.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Entry {
    InodeKey key;
    unsigned char used;
} Entry;

struct InodeSet {
    Entry *tab;
    size_t cap;
    size_t len;
};

static uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static uint64_t key_hash(InodeKey k) {
    uint64_t a = (uint64_t)(uintmax_t)k.dev;
    uint64_t b = (uint64_t)(uintmax_t)k.ino;
    return mix64(a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2)));
}

static int keys_equal(InodeKey a, InodeKey b) {
    return a.dev == b.dev && a.ino == b.ino;
}

static int inode_set_rehash(InodeSet *s, size_t new_cap) {
    Entry *old = s->tab;
    size_t old_cap = s->cap;

    Entry *nt = (Entry *)calloc(new_cap, sizeof(Entry));
    if (!nt) return -1;

    s->tab = nt;
    s->cap = new_cap;
    s->len = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (!old[i].used) continue;
        InodeKey k = old[i].key;

        uint64_t h = key_hash(k);
        size_t idx = (size_t)(h % (uint64_t)new_cap);
        for (;;) {
            if (!s->tab[idx].used) {
                s->tab[idx].used = 1;
                s->tab[idx].key = k;
                s->len++;
                break;
            }
            idx = (idx + 1) % new_cap;
        }
    }

    free(old);
    return 0;
}

InodeSet *inode_set_create(void) {
    InodeSet *s = (InodeSet *)calloc(1, sizeof(InodeSet));
    if (!s) return NULL;

    s->cap = 1024;
    s->tab = (Entry *)calloc(s->cap, sizeof(Entry));
    if (!s->tab) {
        free(s);
        return NULL;
    }
    s->len = 0;
    return s;
}

void inode_set_destroy(InodeSet *set) {
    if (!set) return;
    free(set->tab);
    free(set);
}

bool inode_set_insert(InodeSet *s, InodeKey key, bool *oom) {
    if (oom) *oom = false;
    if (!s) {
        if (oom) *oom = true;
        return false;
    }

    if ((s->len + 1) * 10 >= s->cap * 7) {
        if (inode_set_rehash(s, s->cap * 2) != 0) {
            if (oom) *oom = true;
            return false;
        }
    }

    uint64_t h = key_hash(key);
    size_t idx = (size_t)(h % (uint64_t)s->cap);

    for (;;) {
        if (!s->tab[idx].used) {
            s->tab[idx].used = 1;
            s->tab[idx].key = key;
            s->len++;
            return true;
        }
        if (keys_equal(s->tab[idx].key, key)) return false;
        idx = (idx + 1) % s->cap;
    }
}
