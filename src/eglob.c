#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <assert.h>
#include <ftw.h>
#include <regex.h>
// #include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eglob.h"

static const int MAX_OPEN_FD = 128;
__thread size_t walker_depth = 0;

static int walker(
        const char *fpath,
        const struct stat *sb,
        int typeflag,
        struct FTW *ftwbuf)
{
    /* Depth can't be less than 0. */
    assert(ftwbuf->level >= 0);

    if (ftwbuf->level > walker_depth) {
        walker_depth = ftwbuf->level;
    }

    return 0;
}

int eglob(const char *pattern,
          int flags,
          int (*errfunc) (const char *epath, int eerrno),
          glob_t *pglob)
{
    int status = 0;

    /* Detect '**'. */
    regex_t ss_regex;
    size_t nmatch = 2;
    regmatch_t pmatch[nmatch];

    size_t start = 0;
    size_t end = 0;
    int ss_present = 0;

    status = regcomp(&ss_regex, "[^\\](/[*][*])|^(/[*][*])|^([*][*])", REG_EXTENDED);
    if (status != 0) {
        // fprintf(stderr, "[ERROR] Failed to compile regex.\n");
        regfree(&ss_regex);
        return -1;
    }

    status = regexec(&ss_regex, pattern, nmatch, pmatch, 0);
    if (status != REG_NOMATCH) {
        if (pmatch[1].rm_so != -1) {
            start = pmatch[1].rm_so;
            end = pmatch[1].rm_eo;
        }
        else {
            start = pmatch[0].rm_so;
            end = pmatch[0].rm_eo;
        }

        ss_present = 1;

        // fprintf(stderr, "Start: %zu End: %zu\n", start, end);

        status = regexec(&ss_regex, &pattern[end], nmatch, pmatch, 0);
        if (status != REG_NOMATCH) {
            regfree(&ss_regex);

            /* More than one '**' is NOT permitted. */
            // fprintf(stderr, "[ERROR] Only one '**' per glob is permitted:\n");
            // fprintf(stderr, "[ERROR] %s\n", pattern);
            // fprintf(stderr, "[ERROR] ");
            // for (size_t i = 0; i < (end + pmatch[0].rm_so + 1); i++) {
            //     fprintf(stderr, "-");
            // }
            // fprintf(stderr, "^\n");

            return -2;
        }

    }
    regfree(&ss_regex);

    /* No '**', fallback to normal glob. */
    if (ss_present == 0) {
        return glob(pattern, flags, errfunc, pglob);
    }

    // fprintf(stderr, "Found '**'...\n");

    /* Split glob into pre and post '**'. */
    const size_t pattern_size = strlen(pattern);

    int pre_mashup = 0;
    size_t pre_size = start + 1;
    if (pre_size == 1) {
        pre_size++;
        pre_mashup = 1;
    }

    char pre[pre_size];
    if (pre_mashup == 0) {
        memcpy(pre, pattern, sizeof(pre) - 1);
    }
    else {
        pre[0] = '.';
    }
    pre[sizeof(pre) - 1] = '\0';

    char post[pattern_size - end + 1];
    memcpy(post, pattern + end, sizeof(post) - 1);
    post[sizeof(post) - 1] = '\0';

    /* Glob the prefix path. */
    glob_t prefix_glob = {0, NULL, 0};
    status = glob(pre[0] == '\0' ? "." : pre, flags, errfunc, &prefix_glob);
    if (status != 0) {
        // fprintf(stderr, "[ERROR] Prefix glob failed: %s\n", pre);
        return -1;
    }

    // fprintf(stderr, "Walking %zu paths...\n", prefix_glob.gl_pathc - prefix_glob.gl_offs);

    /* Walk the paths found for prefix to compute longest path. */
    for (size_t i = prefix_glob.gl_offs; i < prefix_glob.gl_pathc; i++) {
        // fprintf(stderr, "Walking[%zu]: %s\n", i, prefix_glob.gl_pathv[i]);
        status = nftw(prefix_glob.gl_pathv[i], walker, MAX_OPEN_FD, FTW_PHYS);
        if (status != 0) {
            // fprintf(stderr, "[ERROR] Walking path failed.\n");
            globfree(&prefix_glob);
            return -1;
        }
    }
    globfree(&prefix_glob);

    // fprintf(stderr, "Max walker depth: %zu\n", walker_depth);

    /* Fold globs. */
    for (size_t i = 0; i <= walker_depth; i++) {
        char extended[sizeof(pre) + sizeof(post) + (i * 2) - 1];

        /* Grow pattern with slash star for each depth. */
        memcpy(extended, pre, sizeof(pre) - 1);
        size_t j = 0;
        for (; j < i; j++) {
            memcpy(&extended[(sizeof(pre) - 1) + (j * 2)], "/*", 2);
        }
        memcpy(&extended[(sizeof(pre) - 1) + (j * 2)], post, sizeof(post));

        // fprintf(stderr, "Processing: %s\n", extended);

        status = glob(extended, flags | GLOB_APPEND, errfunc, pglob);
        if (status != 0 && status != GLOB_NOMATCH) {
            return -1;
        }
    }

    return 0;
}
