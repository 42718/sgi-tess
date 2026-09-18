/*
 * mandelmpi.c — Simple MPI/MPT Mandelbrot tiler for SGI IRIX
 *
 * Rank 0 acts as controller/assembler, other ranks are workers.
 * Output: binary PPM written incrementally, so you can open/refresh it to preview.
 *
 * Build (preferred, if mpicc is available):
 *     mpicc -O3 -o mandelmpi mandelmpi.c
 *
 * Build (MIPSpro cc, if mpicc not present — adjust include/lib paths as needed):
 *     cc -O3 -o mandelmpi mandelmpi.c -lmpi
 *
 * Run example (O2=controller, Origin200 and Octane2 as workers):
 *     mpirun O2host 1 ./mandelmpi : Origin200 6 ./mandelmpi : Octane2 4 ./mandelmpi \
 *            -w 1920 -h 1080 -max 1000 -o /cxfs/out/mandel.ppm -preview 1
 *
 * Notes:
 *  - Uses very simple dynamic work assignment with "row" granularity.
 *  - Rank 0 periodically rewrites the PPM file to allow live preview.
 *  - Keep image on a shared path (CXFS/NFS) if multiple machines are used.
 *  - Code avoids C99 features for better compatibility with older compilers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <mpi.h>

/* Message tags */
#define TAG_WORK  1
#define TAG_DONE  2
#define TAG_DATA  3
#define TAG_STOP  4

/* Clamp to byte */
static unsigned char clampi(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

/* Simple palette (grayscale) */
static void iter_to_rgb(int it, int maxit, unsigned char *r, unsigned char *g, unsigned char *b) {
    int v = (int)(255.0 * ((double)it / (double)maxit));
    unsigned char c = clampi(v);
    *r = c; *g = c; *b = c;
}

/* Compute one row of the Mandelbrot set */
static void compute_row(int y, int width, int height,
                        double xmin, double xmax,
                        double ymin, double ymax,
                        int max_iter,
                        unsigned char *rowbuf) {
    int x;
    for (x = 0; x < width; ++x) {
        double cr = xmin + (xmax - xmin) * (double)x / (double)(width - 1);
        double ci = ymin + (ymax - ymin) * (double)y / (double)(height - 1);
        double zr = 0.0, zi = 0.0;
        int it = 0;
        while (zr*zr + zi*zi <= 4.0 && it < max_iter) {
            double zr2 = zr*zr - zi*zi + cr;
            double zi2 = 2.0*zr*zi + ci;
            zr = zr2; zi = zi2;
            ++it;
        }
        /* Map iterations to RGB */
        iter_to_rgb(it, max_iter, &rowbuf[3*x], &rowbuf[3*x+1], &rowbuf[3*x+2]);
    }
}

/* Write P6 PPM to disk (binary). If "progress_rows" < height, writes partial image. */
static int write_ppm(const char *path, int width, int height, unsigned char *img, int progress_rows) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    /* PPM header */
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    /* Write full height, but use black for rows beyond progress_rows to keep viewer happy */
    if (progress_rows >= height) {
        fwrite(img, 3, width*height, f);
    } else {
        int y;
        /* already computed rows */
        fwrite(img, 3, width*progress_rows, f);
        /* pad remaining rows with black */
        {
            int rem = height - progress_rows;
            unsigned char *zero = (unsigned char*)calloc(3*width, 1);
            if (!zero) { fclose(f); return -1; }
            for (y = 0; y < rem; ++y) fwrite(zero, 3, width, f);
            free(zero);
        }
    }
    fclose(f);
    return 0;
}

/* Parse simple CLI options */
static void parse_args(int argc, char **argv,
                       int *width, int *height, int *max_iter,
                       double *xmin, double *xmax, double *ymin, double *ymax,
                       char *outpath, int outpath_len, int *preview_every) {
    int i;
    /* defaults */
    *width = 1280; *height = 720; *max_iter = 1000;
    *xmin = -2.5; *xmax = 1.0; *ymin = -1.25; *ymax = 1.25;
    *preview_every = 0;
    strncpy(outpath, "mandel.ppm", outpath_len-1); outpath[outpath_len-1] = '\0';

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-w") && i+1 < argc) { *width = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "-h") && i+1 < argc) { *height = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "-max") && i+1 < argc) { *max_iter = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "-xmin") && i+1 < argc) { *xmin = atof(argv[++i]); }
        else if (!strcmp(argv[i], "-xmax") && i+1 < argc) { *xmax = atof(argv[++i]); }
        else if (!strcmp(argv[i], "-ymin") && i+1 < argc) { *ymin = atof(argv[++i]); }
        else if (!strcmp(argv[i], "-ymax") && i+1 < argc) { *ymax = atof(argv[++i]); }
        else if (!strcmp(argv[i], "-o") && i+1 < argc) { strncpy(outpath, argv[++i], outpath_len-1); outpath[outpath_len-1]='\0'; }
        else if (!strcmp(argv[i], "-preview") && i+1 < argc) { *preview_every = atoi(argv[++i]); }
    }
}

int main(int argc, char **argv) {
    int rank, size;
    int width, height, max_iter;
    double xmin, xmax, ymin, ymax;
    char outpath[512];
    int preview_every;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Rank 0 parses options and broadcasts to all */
    if (rank == 0) {
        parse_args(argc, argv, &width, &height, &max_iter, &xmin, &xmax, &ymin, &ymax,
                   outpath, sizeof(outpath), &preview_every);
        if (size < 2) {
            fprintf(stderr, "Need at least 2 MPI ranks (1 controller + >=1 worker).\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&max_iter, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&xmin, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&xmax, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&ymin, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&ymax, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(outpath, 512, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(&preview_every, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        /* Controller/assembler */
        int y_next = 0;
        int active_workers = 0;
        int w;
        unsigned char *image = (unsigned char*)calloc(3*width*height, 1);
        int *row_done = (int*)calloc(height, sizeof(int));
        int rows_completed = 0;
        if (!image || !row_done) {
            fprintf(stderr, "Out of memory.\n");
            MPI_Abort(MPI_COMM_WORLD, 2);
        }

        /* Seed each worker with first row */
        for (w = 1; w < size && y_next < height; ++w) {
            MPI_Send(&y_next, 1, MPI_INT, w, TAG_WORK, MPI_COMM_WORLD);
            ++y_next;
            ++active_workers;
        }

        /* Main receive/dispatch loop */
        while (active_workers > 0) {
            MPI_Status st;
            int row_y;
            /* Probe for data */
            MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            if (st.MPI_TAG == TAG_DATA) {
                /* Receive row data */
                MPI_Recv(&row_y, 1, MPI_INT, st.MPI_SOURCE, TAG_DATA, MPI_COMM_WORLD, &st);
                MPI_Recv(&image[3*width*row_y], 3*width, MPI_UNSIGNED_CHAR, st.MPI_SOURCE, TAG_DATA, MPI_COMM_WORLD, &st);
                if (!row_done[row_y]) {
                    row_done[row_y] = 1;
                    ++rows_completed;
                }
                /* Assign next row or stop */
                if (y_next < height) {
                    MPI_Send(&y_next, 1, MPI_INT, st.MPI_SOURCE, TAG_WORK, MPI_COMM_WORLD);
                    ++y_next;
                } else {
                    int stopmsg = -1;
                    MPI_Send(&stopmsg, 1, MPI_INT, st.MPI_SOURCE, TAG_STOP, MPI_COMM_WORLD);
                    --active_workers;
                }
                /* Periodic preview write */
                if (preview_every > 0 && (rows_completed % preview_every) == 0) {
                    write_ppm(outpath, width, height, image, rows_completed);
                }
            } else if (st.MPI_TAG == TAG_DONE) {
                /* Shouldn't happen in this simple protocol */
                int dummy;
                MPI_Recv(&dummy, 1, MPI_INT, st.MPI_SOURCE, TAG_DONE, MPI_COMM_WORLD, &st);
            } else {
                /* Unknown tag; drain */
                int dummy;
                MPI_Recv(&dummy, 1, MPI_INT, st.MPI_SOURCE, st.MPI_TAG, MPI_COMM_WORLD, &st);
            }
        }

        /* Final write */
        write_ppm(outpath, width, height, image, height);
        free(image);
        free(row_done);
        fprintf(stderr, "Completed %d x %d image. Output: %s\n", width, height, outpath);
    } else {
        /* Worker */
        unsigned char *rowbuf = (unsigned char*)malloc(3*width);
        if (!rowbuf) {
            fprintf(stderr, "Rank %d: out of memory.\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 3);
        }
        for (;;) {
            int row_y;
            MPI_Status st;
            MPI_Recv(&row_y, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            if (st.MPI_TAG == TAG_STOP || row_y < 0) {
                break;
            }
            compute_row(row_y, width, height, xmin, xmax, ymin, ymax, max_iter, rowbuf);
            /* Send back row id and row data */
            MPI_Send(&row_y, 1, MPI_INT, 0, TAG_DATA, MPI_COMM_WORLD);
            MPI_Send(rowbuf, 3*width, MPI_UNSIGNED_CHAR, 0, TAG_DATA, MPI_COMM_WORLD);
        }
        free(rowbuf);
    }

    MPI_Finalize();
    return 0;
}
