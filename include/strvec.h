#ifndef STRVEC_H
#define STRVEC_H

#include <stddef.h>

typedef struct StrVec {
    char **items;
    size_t len;
    size_t cap;
} StrVec;

void strvec_init(StrVec *v);
void strvec_destroy(StrVec *v);

/* Pushes a malloc'd string owned by vec. Returns 0 on success, nonzero on OOM. */
int strvec_push(StrVec *v, char *s);

/* Reads paths from stdin, split by delim ('\n' or '\0'). Returns 0 on success, nonzero on OOM. */
int strvec_read_from_stdin(StrVec *v, int delim);

#endif /* STRVEC_H */
