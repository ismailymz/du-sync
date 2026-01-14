#include "du_sync.h"

#include "path_util.h"
#include "strvec.h"

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out) {
    fprintf(out,
            "Usage: du-sync [OPTIONS] [PATH...]\n"
            "Sums sizes (bytes) of regular files under each PATH.\n"
            "\n"
            "Options:\n"
            "  -0            Read NUL-delimited paths from stdin (only with '-' arg or piped stdin)\n"
            "  -q            Quiet (suppress warnings)\n"
            "  -h, --help    Show this help\n"
            "  -V, --version Show version\n"
            "\n"
            "Input via stdin:\n"
            "  If PATH is '-', read paths from stdin.\n"
            "  If no PATH given and stdin is not a TTY, read paths from stdin.\n");
}

static void version(FILE *out) {
    fprintf(out, "du-sync 1.0.0\n");
}

static int print_one(const char *path, const DuOptions *opt) {
    uint64_t bytes = 0;
    int rc = du_sync_sum_regular_bytes(path, opt, &bytes);
    if (rc != 0) return rc;
    printf("%" PRIu64 "\t%s\n", bytes, path);
    return 0;
}

static int add_stdin_paths(StrVec *paths, int nul_delim) {
    int delim = nul_delim ? '\0' : '\n';
    return strvec_read_from_stdin(paths, delim);
}

int main(int argc, char **argv) {
    DuOptions opt = {.quiet = false, .stdin_nul = false};

    static const struct option long_opts[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {0, 0, 0, 0},
    };

    for (;;) {
        int c = getopt_long(argc, argv, "0qhV", long_opts, NULL);
        if (c == -1) break;
        switch (c) {
            case '0':
                opt.stdin_nul = true;
                break;
            case 'q':
                opt.quiet = true;
                break;
            case 'h':
                usage(stdout);
                return 0;
            case 'V':
                version(stdout);
                return 0;
            default:
                usage(stderr);
                return 2;
        }
    }

    StrVec paths;
    strvec_init(&paths);

    int have_arg_paths = (optind < argc);

    if (!have_arg_paths && !stdin_is_tty()) {
        if (add_stdin_paths(&paths, opt.stdin_nul) != 0) {
            fprintf(stderr, "du-sync: out of memory while reading stdin\n");
            strvec_destroy(&paths);
            return 1;
        }
    } else if (!have_arg_paths) {
        char *dot = xstrdup(".");
        if (!dot || strvec_push(&paths, dot) != 0) {
            free(dot);
            fprintf(stderr, "du-sync: out of memory\n");
            strvec_destroy(&paths);
            return 1;
        }
    } else {
        for (int i = optind; i < argc; i++) {
            if (strcmp(argv[i], "-") == 0) {
                if (add_stdin_paths(&paths, opt.stdin_nul) != 0) {
                    fprintf(stderr, "du-sync: out of memory while reading stdin\n");
                    strvec_destroy(&paths);
                    return 1;
                }
            } else {
                char *p = xstrdup(argv[i]);
                if (!p || strvec_push(&paths, p) != 0) {
                    free(p);
                    fprintf(stderr, "du-sync: out of memory\n");
                    strvec_destroy(&paths);
                    return 1;
                }
            }
        }
    }

    int exit_code = 0;
    for (size_t i = 0; i < paths.len; i++) {
        int rc = print_one(paths.items[i], &opt);
        if (rc != 0) exit_code = 1;
    }

    strvec_destroy(&paths);
    return exit_code;
}