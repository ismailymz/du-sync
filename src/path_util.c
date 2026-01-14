#include "path_util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

char *path_join(const char *base, const char *name) {
    size_t bl = strlen(base);
    size_t nl = strlen(name);

    int need_slash = 1;
    if (bl > 0 && base[bl - 1] == '/') need_slash = 0;

    size_t out_len = bl + (size_t)need_slash + nl + 1;
    char *out = (char *)malloc(out_len);
    if (!out) return NULL;

    memcpy(out, base, bl);
    size_t pos = bl;
    if (need_slash) out[pos++] = '/';
    memcpy(out + pos, name, nl);
    out[pos + nl] = '\0';
    return out;
}

int stdin_is_tty(void) {
    return isatty(STDIN_FILENO) ? 1 : 0;
}
