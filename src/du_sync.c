
#define _XOPEN_SOURCE 700

#include "du_sync.h"
#include "inode_set.h"
#include "path_util.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---------- warning helper ---------- */
static void warn_errno(const DuOptions *opt, const char *msg, const char *path) {
    if (opt && opt->quiet) return;
    fprintf(stderr, "du-sync: %s: %s: %s\n", msg, path, strerror(errno));
}

/* ---------- simple stack for DFS ---------- */
typedef struct {
    char **items;
    size_t len;
    size_t cap;
} PathStack;

static void stack_init(PathStack *s) {
    s->items = NULL;
    s->len = 0;
    s->cap = 0;
}

static void stack_destroy(PathStack *s) {
    for (size_t i = 0; i < s->len; i++) free(s->items[i]);
    free(s->items);
}

static int stack_push(PathStack *s, char *p) {
    if (s->len == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 16;
        char **ni = realloc(s->items, nc * sizeof(*ni));
        if (!ni) return -1;
        s->items = ni;
        s->cap = nc;
    }
    s->items[s->len++] = p;
    return 0;
}

static char *stack_pop(PathStack *s) {
    if (s->len == 0) return NULL;
    return s->items[--s->len];
}

/* ---------- inode-safe file handling ---------- */
static int handle_regular(InodeSet *seen, const struct stat *st, uint64_t *sum) {
    InodeKey key = { .dev = st->st_dev, .ino = st->st_ino };
    bool oom = false;

    if (!inode_set_insert(seen, key, &oom)) return 0;
    if (oom) return -1;

    *sum += (uint64_t)st->st_size;
    return 0;
}

/* ---------- main traversal ---------- */
int du_sync_sum_regular_bytes(const char *root, const DuOptions *opt, uint64_t *out) {
    *out = 0;

    InodeSet *seen = inode_set_create();
    if (!seen) return 1;

    struct stat sb;
    if (lstat(root, &sb) != 0) {
        warn_errno(opt, "cannot stat", root);
        inode_set_destroy(seen);
        return 0;
    }

    if (S_ISREG(sb.st_mode)) {
        handle_regular(seen, &sb, out);
        inode_set_destroy(seen);
        return 0;
    }

    if (!S_ISDIR(sb.st_mode)) {
        inode_set_destroy(seen);
        return 0;
    }

    PathStack st;
    stack_init(&st);

    char *root_copy = xstrdup(root);
    if (!root_copy || stack_push(&st, root_copy) != 0) {
        free(root_copy);
        inode_set_destroy(seen);
        stack_destroy(&st);
        return 1;
    }

    while (1) {
        char *dirpath = stack_pop(&st);
        if (!dirpath) break;

        DIR *dir = opendir(dirpath);
        if (!dir) {
            warn_errno(opt, "cannot open directory", dirpath);
            free(dirpath);
            continue;
        }

        errno = 0;
        struct dirent *de;
        while ((de = readdir(dir))) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;

            char *child = path_join(dirpath, de->d_name);
            if (!child) {
                closedir(dir);
                inode_set_destroy(seen);
                stack_destroy(&st);
                free(dirpath);
                return 1;
            }

            struct stat csb;
            if (lstat(child, &csb) != 0) {
                warn_errno(opt, "cannot stat", child);
                free(child);
                continue;
            }

            if (S_ISDIR(csb.st_mode)) {
                if (stack_push(&st, child) != 0) {
                    free(child);
                    closedir(dir);
                    inode_set_destroy(seen);
                    stack_destroy(&st);
                    free(dirpath);
                    return 1;
                }
            } else if (S_ISREG(csb.st_mode)) {
                if (handle_regular(seen, &csb, out) != 0) {
                    free(child);
                    closedir(dir);
                    inode_set_destroy(seen);
                    stack_destroy(&st);
                    free(dirpath);
                    return 1;
                }
                free(child);
            } else {
                free(child);
            }
        }

        if (errno != 0) warn_errno(opt, "error reading directory", dirpath);

        closedir(dir);
        free(dirpath);
    }

    inode_set_destroy(seen);
    stack_destroy(&st);
    return 0;
}
