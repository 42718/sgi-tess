/*
 * tess_config.c - finding and reading sgi-tess.config. See tess_config.h.
 *
 * C89, libc only.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tess_config.h"

/* Keys only tess-setup uses, accepted here so they are not errors. */
static const char *const setup_keys[] = { "address" };

/* "/a/./b/../c/." -> "/a/c": argv[0] is often "./build/IP27/tess-ui". */
static void tidy_path(char *p)
{
    char *s, *b;
    int n;

    while ((s = strstr(p, "/./")) != (char *)0) {
        memmove(s, s + 2, strlen(s + 2) + 1);
    }
    while ((s = strstr(p, "/../")) != (char *)0 && s > p) {
        for (b = s - 1; b > p && *b != '/'; b--) {
            ;
        }
        memmove(b, s + 3, strlen(s + 3) + 1);
    }
    n = (int)strlen(p);
    while (n >= 2 && p[n - 1] == '.' && p[n - 2] == '/') {
        p[n - 2] = '\0';
        n -= 2;
    }
    while (n > 1 && p[n - 1] == '/') {
        p[--n] = '\0';
    }
}

/* Drop the last path component, in place. 0 if there was one to drop. */
static int up_one(char *p)
{
    char *s = strrchr(p, '/');

    if (!s || s == p) {
        return -1;
    }
    *s = '\0';
    return 0;
}

/* The directory the program was run from, from argv[0] and $PATH. */
static int program_dir(const char *argv0, char *out, int len)
{
    char cand[1024];
    const char *path, *p, *q;
    int n;

    if (!argv0 || !*argv0) {
        return -1;
    }
    if (strchr(argv0, '/')) {
        if (argv0[0] == '/') {
            strncpy(cand, argv0, sizeof cand - 1);
            cand[sizeof cand - 1] = '\0';
        } else {
            if (!getcwd(cand, sizeof cand - 2)) {
                return -1;
            }
            n = (int)strlen(cand);
            strncat(cand, "/", sizeof cand - (size_t)n - 1);
            strncat(cand, argv0, sizeof cand - strlen(cand) - 1);
        }
    } else {
        path = getenv("PATH");
        if (!path) {
            return -1;
        }
        cand[0] = '\0';
        for (p = path; *p; p = *q ? q + 1 : q) {
            q = strchr(p, ':');
            if (!q) {
                q = p + strlen(p);
            }
            n = (int)(q - p);
            if (n <= 0 || n + (int)strlen(argv0) + 2 > (int)sizeof cand) {
                continue;
            }
            memcpy(cand, p, (size_t)n);
            cand[n] = '/';
            strcpy(cand + n + 1, argv0);
            if (access(cand, X_OK) == 0) {
                break;
            }
            cand[0] = '\0';
        }
        if (!cand[0]) {
            return -1;
        }
    }
    tidy_path(cand);
    if (up_one(cand) != 0) {
        return -1;
    }
    strncpy(out, cand, (size_t)len - 1);
    out[len - 1] = '\0';
    return 0;
}

/* Copy src's whitespace-separated words into dst, joined by commas. */
static void join_words(const char *src, char *dst, int len)
{
    const char *p = src;
    int n = 0;

    dst[0] = '\0';
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') {
            p++;
        }
        if (!*p) {
            break;
        }
        if (n > 0 && n < len - 1) {
            dst[n++] = ',';
        }
        while (*p && *p != ' ' && *p != '\t' && *p != ',') {
            if (n < len - 1) {
                dst[n++] = *p;
            }
            p++;
        }
        dst[n] = '\0';
    }
}

static void put(char *dst, const char *val, int len)
{
    strncpy(dst, val, (size_t)len - 1);
    dst[len - 1] = '\0';
}

static void fail(char *err, int errlen, const char *msg)
{
    if (errlen > 0) {
        put(err, msg, errlen);
    }
}

static int parse(TessConfig *c, FILE *f, char *err, int errlen)
{
    char line[1024], key[64], msg[600];
    char *p, *v, *e;
    int lineno = 0, i, known;

    while (fgets(line, sizeof line, f)) {
        lineno++;
        p = strchr(line, '#');
        if (p) {
            *p = '\0';
        }
        e = line + strlen(line);
        while (e > line && (e[-1] == '\n' || e[-1] == '\r' ||
                            e[-1] == ' ' || e[-1] == '\t')) {
            *--e = '\0';
        }
        p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (!*p) {
            continue;
        }
        for (i = 0; *p && *p != ' ' && *p != '\t' && i < (int)sizeof key - 1;
             i++) {
            key[i] = *p++;
        }
        key[i] = '\0';
        v = p;
        while (*v == ' ' || *v == '\t') {
            v++;
        }
        if (!*v && strcmp(key, "address") != 0) {
            sprintf(msg, "%.400s line %d: \"%.40s\" has no value",
                    c->path, lineno, key);
            fail(err, errlen, msg);
            return -1;
        }

        if (strcmp(key, "tree") == 0) {
            put(c->tree, v, (int)sizeof c->tree);
        } else if (strcmp(key, "hosts") == 0) {
            join_words(v, c->hosts, (int)sizeof c->hosts);
        } else if (strcmp(key, "array") == 0) {
            put(c->array, v, (int)sizeof c->array);
        } else if (strcmp(key, "savedir") == 0) {
            put(c->savedir, v, (int)sizeof c->savedir);
        } else if (strcmp(key, "port") == 0) {
            c->port = atoi(v);
            if (c->port <= 0 || c->port > 65535) {
                sprintf(msg, "%.400s line %d: port \"%.40s\" is not a port",
                        c->path, lineno, v);
                fail(err, errlen, msg);
                return -1;
            }
        } else {
            known = 0;
            for (i = 0; i < (int)(sizeof setup_keys / sizeof setup_keys[0]);
                 i++) {
                if (strcmp(key, setup_keys[i]) == 0) {
                    known = 1;
                }
            }
            if (!known) {
                sprintf(msg, "%.400s line %d: unknown key \"%.40s\"",
                        c->path, lineno, key);
                fail(err, errlen, msg);
                return -1;
            }
        }
    }
    return 0;
}

int tess_config_load(TessConfig *c, const char *given, const char *argv0,
                     char *err, int errlen)
{
    char dir[1024], cand[1100], msg[600];
    const char *env;
    FILE *f;
    int i, beside = 0, rc;

    memset(c, 0, sizeof *c);
    if (errlen > 0) {
        err[0] = '\0';
    }

    if (given && *given) {
        put(c->path, given, (int)sizeof c->path);
    } else if ((env = getenv("TESS_CONFIG")) != (const char *)0 && *env) {
        put(c->path, env, (int)sizeof c->path);
    } else if (program_dir(argv0, dir, (int)sizeof dir) == 0) {
        for (i = 0; i < 3; i++) {
            sprintf(cand, "%.1000s/%s", dir, TESS_CONFIG_NAME);
            if (access(cand, F_OK) == 0) {
                put(c->path, cand, (int)sizeof c->path);
                beside = 1;
                break;
            }
            if (up_one(dir) != 0) {
                break;
            }
        }
    }
    if (!c->path[0]) {
        return 0;
    }

    f = fopen(c->path, "r");
    if (!f) {
        sprintf(msg, "cannot read %.400s", c->path);
        fail(err, errlen, msg);
        return -1;
    }
    rc = parse(c, f, err, errlen);
    fclose(f);
    if (rc != 0) {
        return rc;
    }

    /* Found beside the program: the tree is where the config is, unless the
       file says otherwise. */
    if (beside && !c->tree[0]) {
        put(c->tree, c->path, (int)sizeof c->tree);
        (void)up_one(c->tree);
    }
    return 0;
}
