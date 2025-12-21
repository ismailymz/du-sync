#include <stdio.h>
#include <stdlib.h>
#include "traversal.h"

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <directory>\n", prog);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

    const char *root = argv[1];
    long total = 0;

    int rc = du_sequential(root, &total);
    printf("%ld\t%s\n", total, root); 
    if (rc != 0) {
        fprintf(stderr, "du-sync: completed with some errors\n");
        return 2;
    }

    return 0;
}

