#ifndef EGLOB_H
#define EGLOB_H 1

#include <glob.h>

int eglob(const char *pattern,
          int flags,
          int (*errfunc) (const char *epath, int eerrno),
          glob_t *pglob);

#endif /* EGLOB_H */
