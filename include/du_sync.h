#ifndef DU_SYNC_H
#define DU_SYNC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct DuOptions {
    bool quiet;      /* suppress warnings */
    bool stdin_nul;  /* -0: read NUL-delimited paths from stdin */
    int jobs;        /* -j N: number of worker threads (<=1 => sequential) */
    bool debug_threads; /* print thread id*/
} DuOptions;

int du_sync_sum_regular_bytes(const char *root_path, const DuOptions *opt, uint64_t *out_bytes);

#endif /* DU_SYNC_H */
