/*
 * mpi-hello.c — the smallest job that proves a multi-host MPI cluster works.
 *
 * Every rank prints its number and the host it landed on. That is the whole
 * program. Its value is that it is a *real* MPI binary: it calls MPI_Init and
 * stays alive long enough to complete MPT's startup handshake, which is the
 * one thing `mpirun -np 2 hostname` does not do. See doc/MPI-HELLO.md.
 *
 *   cc -64 -mips4 -O2 -o mpi-hello mpi-hello.c -lmpi
 *
 *   setenv MPI_USE_TCP 1                                 # on every host
 *   mpirun -d /usr/people/rutger lucy 2 ./mpi-hello      # local only
 *   mpirun -d /usr/people/rutger aurora 2 ./mpi-hello    # remote only
 *   mpirun -d /usr/people/rutger lucy 1, aurora 1 ./mpi-hello
 *
 * The binary must exist at the same absolute path on every host: MPT does no
 * file staging, and -d is not optional — without it the remote working
 * directory is $HOME, which for root is /.
 *
 * C89 throughout, because MIPSpro means it.
 */

#include <stdio.h>
#include <mpi.h>

int main(int argc, char **argv)
{
    int rank, size, len;
    char name[MPI_MAX_PROCESSOR_NAME];

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(name, &len);

    /* flush explicitly: stdout from several hosts is not synchronised, and a
       rank that dies before exit would otherwise lose its line entirely */
    printf("rank %d of %d on %s\n", rank, size, name);
    fflush(stdout);

    MPI_Finalize();
    return 0;
}
