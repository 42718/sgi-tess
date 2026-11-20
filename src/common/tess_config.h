/*
 * tess_config.h - sgi-tess.config, the site's settings.
 *
 * One file at the top of the tree says where the shared tree is, which
 * machines make up the cluster, which array they are in and which port the
 * master uses. tess-ui and tess-preflight read it here; tess-setup and
 * tess-launch read the same file in shell. Switches on the command line still
 * win over it, and built-in defaults fill whatever it leaves out.
 *
 * The format is what both C and a Bourne shell can read without help:
 *
 *     # comment
 *     tree     /cluster/dev/sgi-tess
 *     hosts    lucy arthur aurora
 *
 * one key and its value per line, "#" to the end of a line is a comment.
 * sgi-tess.config.example at the top of the tree lists every key.
 *
 * C89, libc only: tess-preflight links it.
 */

#ifndef TESS_CONFIG_H
#define TESS_CONFIG_H

#define TESS_CONFIG_NAME "sgi-tess.config"

typedef struct TessConfig {
    char path[512];      /* the file that was read; "" when none was found */
    char tree[512];      /* "" when unset: the config's own directory */
    char hosts[256];     /* comma-separated; "" when unset */
    char array[64];      /* "" when unset: Array Services' default array */
    char savedir[512];   /* "" when unset */
    int  port;           /* 0 when unset */
} TessConfig;

/*
 * Find the config and read it. Looks in order at: given, the argument to
 * -config, when not NULL; $TESS_CONFIG; then beside the program and up to two
 * directories above it, which finds <tree>/sgi-tess.config from
 * <tree>/build/IP27/tess-ui. A config found beside the program with no tree
 * line sets tree to its own directory.
 *
 * Returns 0 when a config was read, or when there is none to read (path is
 * then ""). Returns -1 for a file that exists but cannot be read or has a line
 * it does not understand, with the reason in err. An unknown key is an error,
 * not something skipped: a misspelt "hsots" that was quietly ignored would
 * look exactly like the setting being wrong.
 */
int tess_config_load(TessConfig *c, const char *given, const char *argv0,
                     char *err, int errlen);

#endif /* TESS_CONFIG_H */
