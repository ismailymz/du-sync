#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include "du_sync.h"

#include "inode_set.h"
#include "path_util.h"

#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

static unsigned long long get_tid_ull(void) {
#ifdef SYS_gettid
    return (unsigned long long)syscall(SYS_gettid);
#else
    return (unsigned long long)(uintptr_t)pthread_self();
#endif
}

static void dbg_threads(const DuOptions *opt, const char *fmt, ...) {
    if (!opt || !opt->debug_threads) return;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "du-sync[tid=%llu]: ", get_tid_ull());
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void warn_errno(const DuOptions *opt, const char *msg, const char *path) {
    if (opt && opt->quiet) return;
    fprintf(stderr, "du-sync: %s: %s: %s\n", msg, path, strerror(errno));
}

/* ---------------- Sequential traversal (iterative stack) ---------------- */

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
    s->items = NULL;
    s->len = 0;
    s->cap = 0;
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

static int inode_add_once(InodeSet *seen, const struct stat *st, uint64_t *acc) {
    InodeKey k = {.dev = st->st_dev, .ino = st->st_ino};
    bool oom = false;
    bool inserted = inode_set_insert(seen, k, &oom);
    if (oom) return -1;
    if (inserted) *acc += (uint64_t)st->st_size;
    return 0;
}

static int du_sync_sum_regular_bytes_sequential(const char *root_path, const DuOptions *opt,
                                                uint64_t *out_bytes) {
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
        int rc = inode_add_once(seen, &sb, out_bytes);
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
    if (!root_copy || stack_push(&st, root_copy) != 0) {
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
            if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;

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
                int rc = inode_add_once(seen, &csb, out_bytes);
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

/* ---------------- Parallel traversal (work queue + pthreads) ---------------- */

typedef struct DirNode {
    char *path;
    struct DirNode *next;
} DirNode;

typedef struct WorkQueue {
    DirNode *head;
    DirNode *tail;
    size_t pending; /* queued dirs */
    size_t active;  /* workers currently processing a dir */
    int done;       /* all work complete OR fatal stop */
    pthread_mutex_t mu;
    pthread_cond_t cv;
} WorkQueue;

static void wq_init(WorkQueue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->pending = 0;
    q->active = 0;
    q->done = 0;
    pthread_mutex_init(&q->mu, NULL);
    pthread_cond_init(&q->cv, NULL);
}

static void wq_destroy(WorkQueue *q) {
    pthread_mutex_lock(&q->mu);
    DirNode *cur = q->head;
    while (cur) {
        DirNode *n = cur->next;
        free(cur->path);
        free(cur);
        cur = n;
    }
    q->head = q->tail = NULL;
    q->pending = 0;
    q->active = 0;
    pthread_mutex_unlock(&q->mu);

    pthread_cond_destroy(&q->cv);
    pthread_mutex_destroy(&q->mu);
}

static int wq_push(WorkQueue *q, char *path) {
    DirNode *n = (DirNode *)calloc(1, sizeof(DirNode));
    if (!n) return -1;
    n->path = path;

    pthread_mutex_lock(&q->mu);
    if (q->tail) q->tail->next = n;
    else q->head = n;
    q->tail = n;
    q->pending++;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mu);
    return 0;
}

static char *wq_pop_blocking(WorkQueue *q) {
    pthread_mutex_lock(&q->mu);
    while (!q->done && q->pending == 0) pthread_cond_wait(&q->cv, &q->mu);

    if (q->done) {
        pthread_mutex_unlock(&q->mu);
        return NULL;
    }

    DirNode *n = q->head;
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    q->pending--;
    q->active++;
    pthread_mutex_unlock(&q->mu);

    char *p = n->path;
    free(n);
    return p;
}

static void wq_worker_done(WorkQueue *q) {
    pthread_mutex_lock(&q->mu);
    q->active--;
    if (q->pending == 0 && q->active == 0) {
        q->done = 1;
        pthread_cond_broadcast(&q->cv);
    }
    pthread_mutex_unlock(&q->mu);
}

typedef struct SharedState {
    const DuOptions *opt;
    WorkQueue *q;

    InodeSet *seen;
    pthread_mutex_t seen_mu;

    uint64_t total_bytes;
    pthread_mutex_t total_mu;
} SharedState;

typedef struct WorkerArg {
    SharedState *shared;
    int idx;
} WorkerArg;

static int handle_regular_parallel(SharedState *s, const struct stat *st) {
    InodeKey k = {.dev = st->st_dev, .ino = st->st_ino};
    bool oom = false;

    pthread_mutex_lock(&s->seen_mu);
    bool inserted = inode_set_insert(s->seen, k, &oom);
    pthread_mutex_unlock(&s->seen_mu);

    if (oom) return -1;
    if (!inserted) return 0;

    pthread_mutex_lock(&s->total_mu);
    s->total_bytes += (uint64_t)st->st_size;
    pthread_mutex_unlock(&s->total_mu);
    return 0;
}

static int process_dir_parallel(SharedState *s, const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        warn_errno(s->opt, "cannot open directory", dirpath);
        return 0;
    }

    errno = 0;
    for (struct dirent *de = readdir(dir); de; de = readdir(dir)) {
        const char *name = de->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;

        char *child = path_join(dirpath, name);
        if (!child) {
            closedir(dir);
            return 1; /* fatal OOM */
        }

        struct stat csb;
        if (lstat(child, &csb) != 0) {
            warn_errno(s->opt, "cannot stat", child);
            free(child);
            continue;
        }

        if (S_ISDIR(csb.st_mode)) {
            if (wq_push(s->q, child) != 0) {
                free(child);
                closedir(dir);
                return 1; /* fatal OOM */
            }
            continue;
        }

        if (S_ISREG(csb.st_mode)) {
            int rc = handle_regular_parallel(s, &csb);
            free(child);
            if (rc != 0) {
                closedir(dir);
                return 1; /* fatal OOM */
            }
            continue;
        }

        free(child);
    }

    if (errno != 0) warn_errno(s->opt, "error reading directory", dirpath);

    closedir(dir);
    return 0;
}

static void wq_stop_all(WorkQueue *q) {
    pthread_mutex_lock(&q->mu);
    q->done = 1;
    pthread_cond_broadcast(&q->cv);
    pthread_mutex_unlock(&q->mu);
}

static void *worker_main(void *arg) {
    WorkerArg *wa = (WorkerArg *)arg;
    SharedState *s = wa->shared;
    int idx = wa->idx;

    dbg_threads(s->opt, "worker-start idx=%d", idx);

    for (;;) {
        char *dirpath = wq_pop_blocking(s->q);
        if (!dirpath) break;

        dbg_threads(s->opt, "pop-dir idx=%d path=%s", idx, dirpath);

        int fatal = process_dir_parallel(s, dirpath);
        free(dirpath);

        wq_worker_done(s->q);

        if (fatal) {
            dbg_threads(s->opt, "fatal-oom idx=%d stopping", idx);
            wq_stop_all(s->q);
            break;
        }
    }

    dbg_threads(s->opt, "worker-exit idx=%d", idx);
    return NULL;
}

static int du_sync_sum_regular_bytes_parallel(const char *root_path, const DuOptions *opt,
                                              uint64_t *out_bytes) {
    *out_bytes = 0;

    InodeSet *seen = inode_set_create();
    if (!seen) return 1;

    WorkQueue q;
    wq_init(&q);

    SharedState s = {
        .opt = opt,
        .q = &q,
        .seen = seen,
        .total_bytes = 0,
    };
    pthread_mutex_init(&s.seen_mu, NULL);
    pthread_mutex_init(&s.total_mu, NULL);

    struct stat sb;
    if (lstat(root_path, &sb) != 0) {
        warn_errno(opt, "cannot stat", root_path);
        wq_destroy(&q);
        pthread_mutex_destroy(&s.seen_mu);
        pthread_mutex_destroy(&s.total_mu);
        inode_set_destroy(seen);
        return 0;
    }

    if (S_ISREG(sb.st_mode)) {
        int rc = handle_regular_parallel(&s, &sb);
        *out_bytes = s.total_bytes;
        wq_destroy(&q);
        pthread_mutex_destroy(&s.seen_mu);
        pthread_mutex_destroy(&s.total_mu);
        inode_set_destroy(seen);
        return (rc == 0) ? 0 : 1;
    }

    if (!S_ISDIR(sb.st_mode)) {
        wq_destroy(&q);
        pthread_mutex_destroy(&s.seen_mu);
        pthread_mutex_destroy(&s.total_mu);
        inode_set_destroy(seen);
        return 0;
    }

    char *root_copy = xstrdup(root_path);
    if (!root_copy || wq_push(&q, root_copy) != 0) {
        free(root_copy);
        wq_destroy(&q);
        pthread_mutex_destroy(&s.seen_mu);
        pthread_mutex_destroy(&s.total_mu);
        inode_set_destroy(seen);
        return 1;
    }

    int n = (opt && opt->jobs > 1) ? opt->jobs : 1;

    pthread_t *threads = (pthread_t *)calloc((size_t)n, sizeof(pthread_t));
    WorkerArg *args = (WorkerArg *)calloc((size_t)n, sizeof(WorkerArg));
    if (!threads || !args) {
        free(threads);
        free(args);
        wq_destroy(&q);
        pthread_mutex_destroy(&s.seen_mu);
        pthread_mutex_destroy(&s.total_mu);
        inode_set_destroy(seen);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        args[i].shared = &s;
        args[i].idx = i;
        if (pthread_create(&threads[i], NULL, worker_main, &args[i]) != 0) {
            dbg_threads(opt, "pthread_create failed at idx=%d", i);
            wq_stop_all(&q);
            n = i;
            break;
        }
    }

    for (int i = 0; i < n; i++) pthread_join(threads[i], NULL);

    free(args);
    free(threads);

    *out_bytes = s.total_bytes;

    wq_destroy(&q);
    pthread_mutex_destroy(&s.seen_mu);
    pthread_mutex_destroy(&s.total_mu);
    inode_set_destroy(seen);
    return 0;
}

/* ---------------- Public API ---------------- */

int du_sync_sum_regular_bytes(const char *root_path, const DuOptions *opt, uint64_t *out_bytes) {
    if (!root_path || !out_bytes) return 2;

    int jobs = 1;
    if (opt && opt->jobs > 1) jobs = opt->jobs;

    if (jobs <= 1) return du_sync_sum_regular_bytes_sequential(root_path, opt, out_bytes);
    return du_sync_sum_regular_bytes_parallel(root_path, opt, out_bytes);
}
