/* $Id: mpi_ext.h,v 1.11 2002/11/20 17:14:05 tds Exp $ */
/*
 *	(C) COPYRIGHT SILICON GRAPHICS, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */
#ifndef MPIEXT_H_INCLUDED
#define MPIEXT_H_INCLUDED

#ifdef __linux
#include <sys/types.h>
#else
#include <sgidefs.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif
/*
 * This header file contains defines for some MPI
 * extensions unique to the SGI implementation.
 */
/* Retries allocating mpi per proc headers for collective calls */
#define MPI_SGI_STATS_HDR_PROC_CRETRY 1	

/* Retries allocating mpi per host headers for collective calls */
#define MPI_SGI_STATS_HDR_HOST_CRETRY 2	

/* Retries allocating mpi per proc headers for pt2pt calls */
#define MPI_SGI_STATS_HDR_PROC_PRETRY 3	

/* Retries allocating mpi per host headers for pt2pt calls */
#define MPI_SGI_STATS_HDR_HOST_PRETRY 4	

/* Retries allocating mpi per proc buffers for collective calls */
#define MPI_SGI_STATS_BUF_PROC_CRETRY 5	

/* Retries allocating mpi per host buffers for collective calls */
#define MPI_SGI_STATS_BUF_HOST_CRETRY 6	

/* Retries allocating mpi per proc  buffers for pt2pt calls */
#define MPI_SGI_STATS_BUF_PROC_PRETRY 7	

/* Retries allocating mpi per host  buffers for pt2pt calls */
#define MPI_SGI_STATS_BUF_HOST_PRETRY 8	


/* Send requests using shared memory for collective calls */
#define MPI_SGI_STATS_SENDS_COLL_SHMEM 9

/* Send requests using shared memory for pt2pt calls */
#define MPI_SGI_STATS_SENDS_PT2PT_SHMEM 10

/* Data buffers sent using shared memory for pt2pt calls */
#define MPI_SGI_STATS_DATABUFS_PT2PT_SHMEM 11

/* Bytes sent using single copy for pt2pt calls */
#define MPI_SGI_STATS_SENDS_PT2PT_SINGLE_COPY 12

/* Data buffers sent using shared memory for collective calls */
#define MPI_SGI_STATS_DATABUFS_COLL_SHMEM 13

/* Bytes sent using single copy for collective calls */
#define MPI_SGI_STATS_SENDS_COLL_SINGLE_COPY 14

/* Message headers sent using shared memory for collective calls */
#define MPI_SGI_STATS_HDRS_COLL_SHMEM 15

/* Message headers sent using shared memory for pt2pt calls */
#define MPI_SGI_STATS_HDRS_PT2PT_SHMEM 16

/* Bytes sent using shared memory for pt2pt calls */
#define MPI_SGI_STATS_BYTES_PT2PT_SHMEM 17
#define MPI_SGI_BYTES_PT2PT_SHMEM 17

/* Bytes sent using shared memory for collective calls */
#define MPI_SGI_STATS_BYTES_COLL_SHMEM 18
#define MPI_SGI_BYTES_COLL_SHMEM 18

/* Send requests using hippibypass for collective calls */
#define MPI_SGI_STATS_SENDS_COLL_HIPPI 19

/* Send requests using hippi bypass for pt2pt calls */
#define MPI_SGI_STATS_SENDS_PT2PT_HIPPI 20

/* Data buffers sent using hippi bypass for pt2pt calls */
#define MPI_SGI_STATS_DATABUFS_PT2PT_HIPPI 21

/* Data buffers sent using hippi bypass for collective calls */
#define MPI_SGI_STATS_DATABUFS_COLL_HIPPI 22

/* Message headers sent using hippi bypass for collective calls */
#define MPI_SGI_STATS_HDRS_COLL_HIPPI 23

/* Message headers sent using hippi bypass for pt2pt calls */
#define MPI_SGI_STATS_HDRS_PT2PT_HIPPI 24

/* Bytes sent using hippi bypass for pt2pt calls */
#define MPI_SGI_STATS_BYTES_PT2PT_HIPPI 25
#define MPI_SGI_BYTES_PT2PT_HIPPI 25

/* Bytes sent using hippi bypass for collective calls */
#define MPI_SGI_STATS_BYTES_COLL_HIPPI 26
#define MPI_SGI_BYTES_COLL_HIPPI 26

/* Send requests using tcp/ip for collective calls */
#define MPI_SGI_STATS_SENDS_COLL_TCP 27

/* Send requests using tcp/ip for pt2pt calls */
#define MPI_SGI_STATS_SENDS_PT2PT_TCP 28

/* Data buffers sent using tcp/ip for pt2pt calls */
#define MPI_SGI_STATS_DATABUFS_PT2PT_TCP 29

/* Data buffers sent using tcp/ip for collective calls */
#define MPI_SGI_STATS_DATABUFS_COLL_TCP 30

/* Message headers sent using tcp/ip for collective calls */
#define MPI_SGI_STATS_HDRS_COLL_TCP 31

/* Message headers sent using tcp/ip for pt2pt calls */
#define MPI_SGI_STATS_HDRS_PT2PT_TCP 32

/* Bytes sent using tcp/ip for pt2pt calls */
#define MPI_SGI_STATS_BYTES_PT2PT_TCP 33
#define MPI_SGI_BYTES_PT2PT_TCP 33

/* Bytes sent using tcp/ip for collective calls */
#define MPI_SGI_STATS_BYTES_COLL_TCP 34
#define MPI_SGI_BYTES_COLL_TCP 34

/* Send requests using gsn bypass for collective calls */
#define MPI_SGI_STATS_SENDS_COLL_GSN 35

/* Send requests using gsn bypass for pt2pt calls */
#define MPI_SGI_STATS_SENDS_PT2PT_GSN 36

/* Data buffers sent using gsn bypass for pt2pt calls */
#define MPI_SGI_STATS_DATABUFS_PT2PT_GSN 37

/* Data buffers sent using gsn bypass for collective calls */
#define MPI_SGI_STATS_DATABUFS_COLL_GSN 38

/* Message headers sent using gsn bypass for collective calls */
#define MPI_SGI_STATS_HDRS_COLL_GSN 39

/* Message headers sent using gsn bypass for pt2pt calls */
#define MPI_SGI_STATS_HDRS_PT2PT_GSN 40

/* Bytes sent using gsn bypass for pt2pt calls */
#define MPI_SGI_STATS_BYTES_PT2PT_GSN 41
#define MPI_SGI_BYTES_PT2PT_GSN 41

/* Bytes sent using gsn bypass for collective calls */
#define MPI_SGI_STATS_BYTES_COLL_GSN 42
#define MPI_SGI_BYTES_COLL_GSN 42

/* Send requests using myrinet for collective calls */
#define MPI_SGI_STATS_SENDS_COLL_GM 43

/* Send requests using myrinet for pt2pt calls */
#define MPI_SGI_STATS_SENDS_PT2PT_GM 44

/* Data buffers sent using myrinet for pt2pt calls */
#define MPI_SGI_STATS_DATABUFS_PT2PT_GM 45 

/* Data buffers sent using myrinet for collective calls */
#define MPI_SGI_STATS_DATABUFS_COLL_GM 46

/* Message headers sent using myrinet for collective calls */
#define MPI_SGI_STATS_HDRS_COLL_GM 47

/* Message headers sent using myrinet for pt2pt calls */
#define MPI_SGI_STATS_HDRS_PT2PT_GM 48

/* Bytes sent using myrinet for pt2pt calls */
#define MPI_SGI_STATS_BYTES_PT2PT_GM 49
#define MPI_SGI_BYTES_PT2PT_GM 49

/* Bytes sent using myrinet for collective calls */
#define MPI_SGI_STATS_BYTES_COLL_GM 50
#define MPI_SGI_BYTES_COLL_GM 50

/* Largest value defined in above statistics */
#define MPI_SGI_STAT_MAX 50

#define MPI_SGI_STAT_SUPPORTED 1
#define MPI_SGI_STAT_UNSUPPORTED -1
#define MPI_SGI_STAT_UNDEFINED -2

/* Prototypes */
void MPI_SGI_stat_resetall(void);
void MPI_SGI_stat_reset(int, unsigned int *);
void MPI_SGI_stat_support(unsigned int, unsigned int *, int *);
#ifdef _CRAY
void MPI_SGI_stat_get(unsigned int, unsigned int *, unsigned long *, unsigned long *);
#else
void MPI_SGI_stat_get(unsigned int, unsigned int *, __uint64_t *, __uint64_t *);
#endif
int MPI_SGI_stat_print(int, const char *, const char *);

#if defined(__cplusplus)
}
#endif

#endif
