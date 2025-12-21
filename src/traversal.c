#define _POSIX_C_SOURCE 200809L


#include "traversal.h"
#include "hash_table.h"

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

static int walk_dir(const char *path, long *total, InodeHashSet *set) {
    int had_error = 0;

    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "du-sync: cannot open dir '%s': %s\n", path, strerror(errno));
        return 1; // bu dalda hata var ama üst seviye devam edebilir
    }


    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

        struct stat st;
       if (lstat(fullpath, &st) != 0) {
    fprintf(stderr, "du-sync: cannot stat '%s': %s\n", fullpath, strerror(errno));
    had_error = 1;
    continue;
}


       if (S_ISDIR(st.st_mode)) {
    if (walk_dir(fullpath, total, set) != 0) {
        had_error = 1;
    }
}

 else if (S_ISREG(st.st_mode)) {
    int r = inode_set_check_or_add(set, st.st_dev, st.st_ino);
    if (r == -1) {
        fprintf(stderr, "du-sync: out of memory while tracking inode\n");
        return 1; // ciddi hata
    }
    if (r == 0) {          // yeni inode eklendi
        *total += st.st_size;
    }                      // r==1 ise zaten vardı → sayma
}



    }

    closedir(dir);
    return had_error;
}
int du_sequential(const char *root_path, long *out_total) {
    if (!out_total) return 1;

    InodeHashSet *set = inode_set_create(4096);
    if (!set) {
        fprintf(stderr, "du-sync: cannot allocate inode hash set\n");
        return 1;
    }

    *out_total = 0;
    int rc = walk_dir(root_path, out_total, set);

    inode_set_destroy(set);
    return rc;
}


