#include "strvec.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int strvec_grow(StrVec *v, size_t min_cap) {
    size_t new_cap = (v->cap == 0) ? 8 : v->cap;
    while (new_cap < min_cap) new_cap *= 2;

    char **p = (char **)realloc(v->items, new_cap * sizeof(char *));
    if (!p) return -1;

    v->items = p;
    v->cap = new_cap;
    return 0;
}

void strvec_init(StrVec *v) {
    v->items = NULL;
    v->len = 0;
    v->cap = 0;
}

void strvec_destroy(StrVec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; i++) free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->len = 0;
    v->cap = 0;
}

int strvec_push(StrVec *v, char *s) {
    if (v->len == v->cap) {
        if (strvec_grow(v, v->len + 1) != 0) return -1;
    }
    v->items[v->len++] = s;
    return 0;
}

int strvec_read_from_stdin(StrVec *v, int delim) {
    const size_t CHUNK = 4096;
    unsigned char buf[CHUNK];

    char *cur = NULL;
    size_t cur_len = 0;
    size_t cur_cap = 0;

    for (;;) {
        ssize_t rd = read(STDIN_FILENO, buf, CHUNK);
        if (rd == 0) break;
        if (rd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (ssize_t i = 0; i < rd; i++) {
            unsigned char c = buf[i];

            if ((int)c == delim) {
                if (cur_len == 0) continue;
                char *s = (char *)malloc(cur_len + 1);
                if (!s) goto oom;
                memcpy(s, cur, cur_len);
                s[cur_len] = '\0';
                if (strvec_push(v, s) != 0) {
                    free(s);
                    goto oom;
                }
                cur_len = 0;
                continue;
            }

            if (cur_len + 1 > cur_cap) {
                size_t new_cap = (cur_cap == 0) ? 128 : cur_cap * 2;
                char *p = (char *)realloc(cur, new_cap);
                if (!p) goto oom;
                cur = p;
                cur_cap = new_cap;
            }
            cur[cur_len++] = (char)c;
        }
    }

    if (cur_len > 0) {
        char *s = (char *)malloc(cur_len + 1);
        if (!s) goto oom;
        memcpy(s, cur, cur_len);
        s[cur_len] = '\0';
        if (strvec_push(v, s) != 0) {
            free(s);
            goto oom;
        }
    }

    free(cur);
    return 0;

oom:
    free(cur);
    return -1;
}
