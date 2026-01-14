#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <stddef.h>

/* Joins base + "/" + name (handles base trailing slash). Returns malloc'd string or NULL on OOM. */
char *path_join(const char *base, const char *name);

/* Safe strdup (returns NULL on OOM). */
char *xstrdup(const char *s);

/* Returns true if stdin is a TTY. */
int stdin_is_tty(void);

#endif /* PATH_UTIL_H */
