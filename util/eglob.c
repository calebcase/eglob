#define _GNU_SOURCE

#include <eglob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, const char *argv[])
{
    int status = 0;

    if (argc != 2) {
        return 1;
    }

    glob_t pglob = {0, NULL, 0};
    status = eglob(argv[1], 0, NULL, &pglob);
    if (status != 0) {
        globfree(&pglob);
        return -status;
    }

    /*
    qsort(pglob.gl_pathv,
          pglob.gl_pathc,
          sizeof(pglob.gl_pathv[0]),
          (int(*)(const void *, const void *))strverscmp);
    */

    for (size_t i = 0; i < pglob.gl_pathc; i++) {
        printf("%s\n", pglob.gl_pathv[i]);
    }

    globfree(&pglob);

    return 0;
}
