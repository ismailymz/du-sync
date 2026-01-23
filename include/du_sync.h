#ifndef DU_SYNC_H
#define DU_SYNC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct DuOptions {
    bool quiet;
    bool stdin_nul;
    int jobs;
    bool debug_threads;
} DuOptions;

int du_sync_sum_regular_bytes(const char *root_path, const DuOptions *opt, uint64_t *out_bytes);

#endif /* DU_SYNC_H */
