/*
 * tess_paramdesc.h - a parameter descriptor, with no Motif in it.
 *
 * Moved out of src/ui/tess_params.h for build 6, so a compute module in
 * src/common can carry its own parameter table without dragging X headers onto
 * the workers. tess_params.h still builds the pane from these; this file only
 * says what a parameter is.
 *
 * C89.
 */

#ifndef TESS_PARAMDESC_H
#define TESS_PARAMDESC_H

typedef enum {
    TESS_P_DOUBLE,
    TESS_P_INT,
    TESS_P_BOOL,
    TESS_P_ENUM
} TessParamType;

#define TESS_CLUSTER 0      /* changing it means a new epoch */
#define TESS_LOCAL   1      /* changing it re-shades, and costs no network */

#define TESS_MAX_ENUM 8

typedef struct TessParamDesc {
    const char   *label;
    TessParamType type;
    int           offset;            /* byte offset into the values struct */
    double        lo, hi;            /* clamp; ignored for BOOL */
    int           where;             /* TESS_CLUSTER or TESS_LOCAL */
    const char   *names[TESS_MAX_ENUM];  /* ENUM only, NULL-terminated */
    int           digits;            /* DOUBLE: significant digits shown,
                                        0 for the default 12 */
    int           beside;            /* 1: on the same row as the one
                                        before, each taking half. A label
                                        of "" means no label: the control
                                        takes the whole row */
} TessParamDesc;

#endif /* TESS_PARAMDESC_H */
