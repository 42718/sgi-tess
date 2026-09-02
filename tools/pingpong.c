/*
 * pingpong.c — MPI round-trip latency and bandwidth between two ranks.
 *
 * The 12288-byte size is one 64x64 tile at 3 bytes/pixel, i.e. exactly what
 * Tess will send. That number, over each fabric, decides whether the raw
 * low-latency link work is worth doing at all.
 *
 *   cc -64 -mips4 -O2 -o pingpong pingpong.c -lmpi -lm
 *
 *   mpirun -v -a tess     -d /work/tess lucy 1, arthur 1 ./pingpong   # HIPPI
 *   mpirun -v -a tess     -d /work/tess lucy 1, aurora 1 ./pingpong   # Myrinet
 *   mpirun -v -a tess-eth -d /work/tess lucy 1, arthur 1 ./pingpong   # ethernet
 *   mpirun -v -np 2 ./pingpong                                        # on-host shmem
 *
 * C89 throughout, because MIPSpro means it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

static double bench(int rank, int peer, int bytes, int reps)
{
    char *buf;
    int i;
    double t0, t1;
    MPI_Status st;

    buf = (char *)malloc((size_t)bytes);
    if (!buf) {
        fprintf(stderr, "pingpong: out of memory at %d bytes\n", bytes);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    memset(buf, 0x5a, (size_t)bytes);

    /* one untimed exchange so any lazy connection setup is not measured */
    if (rank == 0) {
        MPI_Send(buf, bytes, MPI_BYTE, peer, 1, MPI_COMM_WORLD);
        MPI_Recv(buf, bytes, MPI_BYTE, peer, 2, MPI_COMM_WORLD, &st);
    } else {
        MPI_Recv(buf, bytes, MPI_BYTE, peer, 1, MPI_COMM_WORLD, &st);
        MPI_Send(buf, bytes, MPI_BYTE, peer, 2, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    t0 = MPI_Wtime();
    for (i = 0; i < reps; ++i) {
        if (rank == 0) {
            MPI_Send(buf, bytes, MPI_BYTE, peer, 1, MPI_COMM_WORLD);
            MPI_Recv(buf, bytes, MPI_BYTE, peer, 2, MPI_COMM_WORLD, &st);
        } else {
            MPI_Recv(buf, bytes, MPI_BYTE, peer, 1, MPI_COMM_WORLD, &st);
            MPI_Send(buf, bytes, MPI_BYTE, peer, 2, MPI_COMM_WORLD);
        }
    }
    t1 = MPI_Wtime();
    free(buf);
    return (t1 - t0) / (double)reps;        /* seconds per round trip */
}

int main(int argc, char **argv)
{
    int rank, size, i, reps;
    int sizes[5];
    double rt;
    char name[MPI_MAX_PROCESSOR_NAME];
    int namelen;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2) {
        if (rank == 0)
            fprintf(stderr, "pingpong: need exactly 2 ranks, got %d\n", size);
        MPI_Finalize();
        return 1;
    }

    MPI_Get_processor_name(name, &namelen);
    if (rank == 0) {
        printf("pingpong: rank 0 on %s\n", name);
        printf("    bytes        rtt us     half us      MB/s\n");
        fflush(stdout);
    }

    sizes[0] = 8;                    /* latency floor                     */
    sizes[1] = 12288;                /* one 64x64 tile at 3 B/px          */
    sizes[2] = 49152;                /* one 128x128 tile                  */
    sizes[3] = 262144;
    sizes[4] = 4194304;              /* bulk, for bandwidth               */

    for (i = 0; i < 5; ++i) {
        reps = (sizes[i] > 262144) ? 50 : 1000;
        rt = bench(rank, 1 - rank, sizes[i], reps);
        if (rank == 0) {
            printf("%9d  %12.1f %11.1f %9.1f\n",
                   sizes[i], rt * 1e6, rt * 5e5,
                   (2.0 * (double)sizes[i] / rt) / 1048576.0);
            fflush(stdout);
        }
    }

    MPI_Finalize();
    return 0;
}
