#ifndef DU_SYNC_H
#define DU_SYNC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct DuOptions {
    bool quiet;           /* suppress warnings */
    bool stdin_nul;       /* -0: read NUL-delimited paths from stdin */
} DuOptions;

/*
 * Computes the total size (bytes) of regular files under `root_path`.
 * - Uses lstat() (does not follow symlinks).
 * - Avoids double-counting hard links by tracking (st_dev, st_ino).
 * - On errors, prints warning to stderr (unless quiet) and continues.
 *
 * Returns 0 on success (even if some sub-entries fail), nonzero only on fatal
 * allocation errors.
 */
int du_sync_sum_regular_bytes(const char *root_path, const DuOptions *opt, uint64_t *out_bytes);

#endif /* DU_SYNC_H */