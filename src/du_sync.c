#define _XOPEN_SOURCE 700
#include "du_sync.h"

#include "inode_set.h"
#include "path_util.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct PathStack {
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
    if (!s) return;
    for (size_t i = 0; i < s->len; i++) free(s->items[i]);
    free(s->items);
}

static int stack_push(PathStack *s, char *path) {
    if (s->len == s->cap) {
        size_t new_cap = (s->cap == 0) ? 16 : s->cap * 2;
        char **p = (char **)realloc(s->items, new_cap * sizeof(char *));
        if (!p) return -1;
        s->items = p;
        s->cap = new_cap;
    }
    s->items[s->len++] = path;
    return 0;
}

static char *stack_pop(PathStack *s) {
    if (s->len == 0) return NULL;
    return s->items[--s->len];
}

static void warn_errno(const DuOptions *opt, const char *msg, const char *path) {
    if (opt && opt->quiet) return;
    if (path) {
        fprintf(stderr, "du-sync: %s: %s: %s\n", msg, path, strerror(errno));
    } else {
        fprintf(stderr, "du-sync: %s: %s\n", msg, strerror(errno));
    }
}

static int handle_regular(const DuOptions *opt, InodeSet *seen, const struct stat *st, uint64_t *acc) {
    (void)opt;
    InodeKey k = {.dev = st->st_dev, .ino = st->st_ino};
    bool oom = false;
    bool inserted = inode_set_insert(seen, k, &oom);
    if (oom) return -1;
    if (inserted) *acc += (uint64_t)st->st_size;
    return 0;
}

int du_sync_sum_regular_bytes(const char *root_path, const DuOptions *opt, uint64_t *out_bytes) {
    if (!root_path || !out_bytes) return 2;

    *out_bytes = 0;

    InodeSet *seen = inode_set_create();
    if (!seen) return 1;

    PathStack st;
    stack_init(&st);

    struct stat sb;
    if (lstat(root_path, &sb) != 0) {
        warn_errno(opt, "cannot stat", root_path);
        inode_set_destroy(seen);
        stack_destroy(&st);
        return 0;
    }

    if (S_ISREG(sb.st_mode)) {
        int rc = handle_regular(opt, seen, &sb, out_bytes);
        inode_set_destroy(seen);
        stack_destroy(&st);
        return (rc == 0) ? 0 : 1;
    }

    if (!S_ISDIR(sb.st_mode)) {
        inode_set_destroy(seen);
        stack_destroy(&st);
        return 0;
    }

    char *root_copy = xstrdup(root_path);
    if (!root_copy) {
        inode_set_destroy(seen);
        stack_destroy(&st);
        return 1;
    }
    if (stack_push(&st, root_copy) != 0) {
        free(root_copy);
        inode_set_destroy(seen);
        stack_destroy(&st);
        return 1;
    }

    for (;;) {
        char *dirpath = stack_pop(&st);
        if (!dirpath) break;

        DIR *dir = opendir(dirpath);
        if (!dir) {
            warn_errno(opt, "cannot open directory", dirpath);
            free(dirpath);
            continue;
        }

        errno = 0;
        for (struct dirent *de = readdir(dir); de; de = readdir(dir)) {
            const char *name = de->d_name;
            if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
                continue;
            }

            char *child = path_join(dirpath, name);
            if (!child) {
                closedir(dir);
                free(dirpath);
                inode_set_destroy(seen);
                stack_destroy(&st);
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
                    free(dirpath);
                    inode_set_destroy(seen);
                    stack_destroy(&st);
                    return 1;
                }
                continue;
            }

            if (S_ISREG(csb.st_mode)) {
                int rc = handle_regular(opt, seen, &csb, out_bytes);
                free(child);
                if (rc != 0) {
                    closedir(dir);
                    free(dirpath);
                    inode_set_destroy(seen);
                    stack_destroy(&st);
                    return 1;
                }
                continue;
            }

            free(child);
        }

        if (errno != 0) warn_errno(opt, "error reading directory", dirpath);

        closedir(dir);
        free(dirpath);
    }

    inode_set_destroy(seen);
    stack_destroy(&st);
    return 0;
}
