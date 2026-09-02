/* $Id: mpi.h,v 1.33 2003/03/24 17:12:48 tmiller Exp $ */

/*
 *	(C) COPYRIGHT SILICON GRAPHICS, INC.
 *	UNPUBLISHED PROPRIETARY INFORMATION.
 *	ALL RIGHTS RESERVED.
 */

/*
 * Copyright Notice
 *  + 1993 University of Chicago
 *  + 1993 Mississippi State University
 */

#ifndef MPI_H_INCLUDED
#define MPI_H_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

/******************/
/* MPI-1 bindings */
/******************/


#define MPI_BOTTOM		((MPI_Aint)0)

typedef long			MPI_Aint;
typedef unsigned int		MPI_Request;
typedef unsigned int		MPI_Group;
typedef unsigned int		MPI_Comm;
typedef unsigned int		MPI_Errhandler;
typedef unsigned int		MPI_Op;
typedef unsigned int		MPI_Datatype;
typedef unsigned int 		MPI_Win;

typedef struct { 
	int MPI_SOURCE;
	int MPI_TAG;
	int MPI_ERROR;
	int size;
	int reserved[2];
} MPI_Status;

extern MPI_Status mpi_sgi_status_ignore;
#define MPI_STATUS_IGNORE (&mpi_sgi_status_ignore)
#define MPI_STATUSES_IGNORE (MPI_STATUS_IGNORE)

extern int mpi_sgi_inplace;
#define MPI_IN_PLACE ( (void*)&mpi_sgi_inplace)

enum {
	MPI_COMM_NULL		= 0,
	MPI_COMM_WORLD		= 1,
	MPI_COMM_SELF		= 2
};

enum {
	MPI_ERRHANDLER_NULL	= 0,
	MPI_ERRORS_ARE_FATAL	= 1,
	MPI_ERRORS_RETURN	= 2
};

enum {
	MPI_GROUP_NULL		= 0,
	MPI_GROUP_EMPTY		= 1
};

enum {
	MPI_REQUEST_NULL	= 0
};

enum {
	MPI_OP_NULL		= 0,
	MPI_MAX			= 1,
	MPI_MIN			= 2,
	MPI_SUM			= 3,
	MPI_PROD		= 4,
	MPI_LAND		= 5,
	MPI_BAND 		= 6,
	MPI_LOR			= 7,
	MPI_BOR			= 8,
	MPI_LXOR		= 9,
	MPI_BXOR		= 10,
	MPI_MAXLOC		= 11,
	MPI_MINLOC		= 12
};

enum {
	MPI_DATATYPE_NULL	= 0,

	MPI_CHAR		= 1,
	MPI_SHORT		= 2,
	MPI_INT			= 3,
	MPI_LONG		= 4,
	MPI_UNSIGNED_CHAR	= 5,
	MPI_UNSIGNED_SHORT	= 6,
	MPI_UNSIGNED		= 7,
	MPI_UNSIGNED_LONG	= 8,
	MPI_FLOAT		= 9,
	MPI_DOUBLE		= 10,
	MPI_LONG_DOUBLE		= 11,
	MPI_LONG_LONG		= 12,

	MPI_INTEGER		= 13,
	MPI_REAL		= 14,
	MPI_DOUBLE_PRECISION	= 15,
	MPI_COMPLEX		= 16,
	MPI_DOUBLE_COMPLEX	= 17,
	MPI_LOGICAL		= 18,
	MPI_CHARACTER		= 19,
	MPI_INTEGER1		= 20,
	MPI_INTEGER2		= 21,
	MPI_INTEGER4		= 22,
	MPI_INTEGER8		= 23,
	MPI_REAL4		= 24,
	MPI_REAL8		= 25,
	MPI_REAL16		= 26,

	MPI_BYTE		= 27,
	MPI_PACKED		= 28,
	MPI_UB			= 29,
	MPI_LB			= 30,

	MPI_FLOAT_INT		= 31,
	MPI_DOUBLE_INT		= 32,
	MPI_LONG_INT		= 33,
	MPI_2INT		= 34,
	MPI_SHORT_INT		= 35,
	MPI_LONG_DOUBLE_INT	= 36,

	MPI_2REAL		= 37,
	MPI_2DOUBLE_PRECISION	= 38,
	MPI_2INTEGER		= 39,
	_MPI_SGI_TYPE_LAST
};

#define MPI_LONG_LONG_INT	MPI_LONG_LONG

enum {
	MPI_SUCCESS			= 0,

	/* These 19 error codes are specified by the MPI-1 standard */

	MPI_ERR_BUFFER			= 1,
	MPI_ERR_COUNT			= 2,
	MPI_ERR_TYPE			= 3,
	MPI_ERR_TAG			= 4,
	MPI_ERR_COMM			= 5,
	MPI_ERR_RANK			= 6,
	MPI_ERR_REQUEST			= 7,
	MPI_ERR_ROOT			= 8,
	MPI_ERR_GROUP			= 9,
	MPI_ERR_OP			= 10,
	MPI_ERR_TOPOLOGY		= 11,
	MPI_ERR_DIMS			= 12,
	MPI_ERR_ARG			= 13,
	MPI_ERR_UNKNOWN			= 14,
	MPI_ERR_TRUNCATE		= 15,
	MPI_ERR_OTHER			= 16,
	MPI_ERR_INTERN			= 17,
	MPI_ERR_IN_STATUS		= 18,
	MPI_ERR_PENDING			= 19,


	/* Error codes 20-27 used by MPI on T3E systems. */

	/* These 34 error codes are specified by the MPI-2 standard */

	MPI_ERR_ACCESS			= 28,
	MPI_ERR_AMODE			= 29,
	MPI_ERR_ASSERT			= 30,
	MPI_ERR_BAD_FILE		= 31,
	MPI_ERR_BASE			= 32,
	MPI_ERR_CONVERSION		= 33,
	MPI_ERR_DISP			= 34,
	MPI_ERR_DUP_DATAREP		= 35,
	MPI_ERR_FILE_EXISTS		= 36,
	MPI_ERR_FILE_IN_USE		= 37,
	MPI_ERR_FILE			= 38,
	MPI_ERR_INFO_KEY		= 39,
	MPI_ERR_INFO_NOKEY		= 40,
	MPI_ERR_INFO_VALUE		= 41,
	MPI_ERR_INFO			= 42,
	MPI_ERR_IO			= 43,
	MPI_ERR_KEYVAL			= 44,
	MPI_ERR_LOCKTYPE		= 45,
	MPI_ERR_NAME			= 46,
	MPI_ERR_NO_MEM			= 47,
	MPI_ERR_NOT_SAME		= 48,
	MPI_ERR_NO_SPACE		= 49,
	MPI_ERR_NO_SUCH_FILE		= 50,
	MPI_ERR_PORT			= 51,
	MPI_ERR_QUOTA			= 52,
	MPI_ERR_READ_ONLY		= 53,
	MPI_ERR_RMA_CONFLICT		= 54,
	MPI_ERR_RMA_SYNC		= 55,
	MPI_ERR_SERVICE			= 56,
	MPI_ERR_SIZE			= 57,
	MPI_ERR_SPAWN			= 58,
	MPI_ERR_UNSUPPORTED_DATAREP	= 59,
	MPI_ERR_UNSUPPORTED_OPERATION	= 60,
	MPI_ERR_WIN			= 61,
	MPI_ERR_LASTCODE		= 100	/* last built-in error code */
};

enum {
	MPI_KEYVAL_INVALID	= 0,
	MPI_TAG_UB		= 1,
	MPI_HOST		= 2,
	MPI_IO			= 3,
	MPI_WTIME_IS_GLOBAL	= 4,
	MPI_UNIVERSE_SIZE	= 9,
	MPI_APPNUM		= 11
};

enum {
	MPI_IDENT		= 0,
	MPI_CONGRUENT		= 1,
	MPI_SIMILAR		= 2,
	MPI_UNEQUAL		= 3
};

enum {
	MPI_GRAPH		= 1,
	MPI_CART		= 2
};

enum {
	MPI_UNDEFINED		= -3,
	MPI_ANY_SOURCE		= -2,
	MPI_PROC_NULL		= -1
};

enum {
	MPI_ANY_TAG		= -1
};

enum {
	MPI_BSEND_OVERHEAD	= 32
};

enum {
	MPI_MAX_PROCESSOR_NAME	= 256
};

enum {
	MPI_MAX_ERROR_STRING	= 256
};

enum {
	MPI_LOCK_SHARED = 1,
	MPI_LOCK_EXCLUSIVE = 2
};


typedef int MPI_Copy_function(MPI_Comm, int, void *, void *, void *, int *);
typedef int MPI_Delete_function(MPI_Comm, int, void *, void *);
typedef void MPI_Handler_function(MPI_Comm *, int *, ...);
typedef void MPI_User_function(void *, void *, int *, MPI_Datatype *); 

MPI_Copy_function		MPI_NULL_COPY_FN, MPI_DUP_FN;
MPI_Delete_function		MPI_NULL_DELETE_FN;

typedef int MPI_Comm_copy_attr_function(MPI_Comm, int, void *, void *, void *, int *);
typedef int MPI_Comm_delete_attr_function(MPI_Comm, int, void *, void *);

MPI_Comm_copy_attr_function             MPI_COMM_NULL_COPY_FN, MPI_COMM_DUP_FN;
MPI_Comm_delete_attr_function           MPI_COMM_NULL_DELETE_FN;

typedef int MPI_Win_copy_attr_function(MPI_Win, int, void *, void *, void *, int *);
typedef int MPI_Win_delete_attr_function(MPI_Win, int, void *, void *);

MPI_Win_copy_attr_function             MPI_WIN_NULL_COPY_FN, MPI_WIN_DUP_FN;
MPI_Win_delete_attr_function           MPI_WIN_NULL_DELETE_FN;

typedef int MPI_Type_copy_attr_function(MPI_Datatype, int, void *, void *, void *, int *);
typedef int MPI_Type_delete_attr_function(MPI_Datatype, int, void *, void *);

MPI_Type_copy_attr_function             MPI_TYPE_NULL_COPY_FN, MPI_TYPE_DUP_FN;
MPI_Type_delete_attr_function           MPI_TYPE_NULL_DELETE_FN;

/*************************************/
/* MPI-1 bindings, sorted by chapter */
/*************************************/


/* 3.2 */

int  MPI_Send(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Send(void *, int, MPI_Datatype, int, int, MPI_Comm);

int  MPI_Recv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);
int PMPI_Recv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);

int  MPI_Get_count(MPI_Status *, MPI_Datatype, int *);
int PMPI_Get_count(MPI_Status *, MPI_Datatype, int *);

/* 3.4 */

int  MPI_Bsend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Bsend(void *, int, MPI_Datatype, int, int, MPI_Comm);

int  MPI_Ssend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Ssend(void *, int, MPI_Datatype, int, int, MPI_Comm);

int  MPI_Rsend(void *, int, MPI_Datatype, int, int, MPI_Comm);
int PMPI_Rsend(void *, int, MPI_Datatype, int, int, MPI_Comm);

/* 3.6 */

int  MPI_Buffer_attach(void *, int);
int PMPI_Buffer_attach(void *, int);

int  MPI_Buffer_detach(void *, int *);
int PMPI_Buffer_detach(void *, int *);

/* 3.7 */

int  MPI_Isend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Isend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Ibsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Ibsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Issend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Issend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Irsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Irsend(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Irecv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Irecv(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Wait(MPI_Request *, MPI_Status *);
int PMPI_Wait(MPI_Request *, MPI_Status *);

int  MPI_Test(MPI_Request *, int *, MPI_Status *);
int PMPI_Test(MPI_Request *, int *, MPI_Status *);

int  MPI_Request_free(MPI_Request *);
int PMPI_Request_free(MPI_Request *);

int  MPI_Waitany(int, MPI_Request *, int *, MPI_Status *);
int PMPI_Waitany(int, MPI_Request *, int *, MPI_Status *);

int  MPI_Testany(int, MPI_Request *, int *, int *, MPI_Status *);
int PMPI_Testany(int, MPI_Request *, int *, int *, MPI_Status *);

int  MPI_Waitall(int, MPI_Request *, MPI_Status *);
int PMPI_Waitall(int, MPI_Request *, MPI_Status *);

int  MPI_Testall(int, MPI_Request *, int *, MPI_Status *);
int PMPI_Testall(int, MPI_Request *, int *, MPI_Status *);

int  MPI_Waitsome(int, MPI_Request *, int *, int *, MPI_Status *);
int PMPI_Waitsome(int, MPI_Request *, int *, int *, MPI_Status *);

int  MPI_Testsome(int, MPI_Request *, int *, int *, MPI_Status *);
int PMPI_Testsome(int, MPI_Request *, int *, int *, MPI_Status *);

/* 3.8 */

int  MPI_Iprobe(int, int, MPI_Comm, int *, MPI_Status *);
int PMPI_Iprobe(int, int, MPI_Comm, int *, MPI_Status *);

int  MPI_Probe(int, int, MPI_Comm, MPI_Status *);
int PMPI_Probe(int, int, MPI_Comm, MPI_Status *);

int  MPI_Cancel(MPI_Request *);
int PMPI_Cancel(MPI_Request *);

int  MPI_Test_cancelled(MPI_Status *, int *);
int PMPI_Test_cancelled(MPI_Status *, int *);

/* 3.9 */

int  MPI_Send_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Send_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Bsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Bsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Ssend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Ssend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Rsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Rsend_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Recv_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);
int PMPI_Recv_init(void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Request *);

int  MPI_Start(MPI_Request *);
int PMPI_Start(MPI_Request *);

int  MPI_Startall(int, MPI_Request *);
int PMPI_Startall(int, MPI_Request *);

/* 3.10 */

int  MPI_Sendrecv(void *, int, MPI_Datatype, int, int, void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);
int PMPI_Sendrecv(void *, int, MPI_Datatype, int, int, void *, int, MPI_Datatype, int, int, MPI_Comm, MPI_Status *);

int  MPI_Sendrecv_replace(void *, int, MPI_Datatype, int, int, int, int, MPI_Comm, MPI_Status *);
int PMPI_Sendrecv_replace(void *, int, MPI_Datatype, int, int, int, int, MPI_Comm, MPI_Status *);

/* 3.12 */

int  MPI_Type_contiguous(int, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_contiguous(int, MPI_Datatype, MPI_Datatype *);

int  MPI_Type_vector(int, int, int, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_vector(int, int, int, MPI_Datatype, MPI_Datatype *);

int  MPI_Type_hvector(int, int, MPI_Aint, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_hvector(int, int, MPI_Aint, MPI_Datatype, MPI_Datatype *);

int  MPI_Type_indexed(int, int *, int *, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_indexed(int, int *, int *, MPI_Datatype, MPI_Datatype *);

int  MPI_Type_hindexed(int, int *, MPI_Aint *, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_hindexed(int, int *, MPI_Aint *, MPI_Datatype, MPI_Datatype *);

int  MPI_Type_struct(int, int *, MPI_Aint *, MPI_Datatype *, MPI_Datatype *);
int PMPI_Type_struct(int, int *, MPI_Aint *, MPI_Datatype *, MPI_Datatype *);

int  MPI_Address(void *, MPI_Aint *);
int PMPI_Address(void *, MPI_Aint *);

int  MPI_Type_extent(MPI_Datatype, MPI_Aint *);
int PMPI_Type_extent(MPI_Datatype, MPI_Aint *);

int  MPI_Type_size(MPI_Datatype, int *);
int PMPI_Type_size(MPI_Datatype, int *);

int  MPI_Type_lb(MPI_Datatype, MPI_Aint *);
int PMPI_Type_lb(MPI_Datatype, MPI_Aint *);

int  MPI_Type_ub(MPI_Datatype, MPI_Aint *);
int PMPI_Type_ub(MPI_Datatype, MPI_Aint *);

int  MPI_Type_commit(MPI_Datatype *);
int PMPI_Type_commit(MPI_Datatype *);

int  MPI_Type_free(MPI_Datatype *);
int PMPI_Type_free(MPI_Datatype *);

int  MPI_Get_elements(MPI_Status *, MPI_Datatype, int *);
int PMPI_Get_elements(MPI_Status *, MPI_Datatype, int *);

/* 3.13 */

int  MPI_Pack(void *, int, MPI_Datatype, void *, int, int *, MPI_Comm);
int PMPI_Pack(void *, int, MPI_Datatype, void *, int, int *, MPI_Comm);

int  MPI_Unpack(void *, int, int *, void *, int, MPI_Datatype, MPI_Comm);
int PMPI_Unpack(void *, int, int *, void *, int, MPI_Datatype, MPI_Comm);

int  MPI_Pack_size(int, MPI_Datatype, MPI_Comm, int *);
int PMPI_Pack_size(int, MPI_Datatype, MPI_Comm, int *);

/* 4.3 */

int  MPI_Barrier(MPI_Comm);
int PMPI_Barrier(MPI_Comm);

/* 4.4 */

int  MPI_Bcast(void *, int, MPI_Datatype, int, MPI_Comm);
int PMPI_Bcast(void *, int, MPI_Datatype, int, MPI_Comm);

/* 4.5 */

int  MPI_Gather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm); 
int PMPI_Gather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm); 

int  MPI_Gatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, int, MPI_Comm); 
int PMPI_Gatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, int, MPI_Comm); 

/* 4.6 */

int  MPI_Scatter(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int PMPI_Scatter(void *, int, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);

int  MPI_Scatterv(void *, int *, int *, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);
int PMPI_Scatterv(void *, int *, int *, MPI_Datatype, void *, int, MPI_Datatype, int, MPI_Comm);

/* 4.7 */

int  MPI_Allgather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);
int PMPI_Allgather(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);

int  MPI_Allgatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);
int PMPI_Allgatherv(void *, int, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);

/* 4.8 */

int  MPI_Alltoall(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);
int PMPI_Alltoall(void *, int, MPI_Datatype, void *, int, MPI_Datatype, MPI_Comm);

int  MPI_Alltoallv(void *, int *, int *, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);
int PMPI_Alltoallv(void *, int *, int *, MPI_Datatype, void *, int *, int *, MPI_Datatype, MPI_Comm);

/* 4.9 */

int  MPI_Reduce(void *, void *, int, MPI_Datatype, MPI_Op, int, MPI_Comm);
int PMPI_Reduce(void *, void *, int, MPI_Datatype, MPI_Op, int, MPI_Comm);

int  MPI_Op_create(MPI_User_function *, int, MPI_Op *);
int PMPI_Op_create(MPI_User_function *, int, MPI_Op *);

int  MPI_Op_free(MPI_Op *);
int PMPI_Op_free(MPI_Op *);

int  MPI_Allreduce(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);
int PMPI_Allreduce(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);

/* 4.10 */

int  MPI_Reduce_scatter(void *, void *, int *, MPI_Datatype, MPI_Op, MPI_Comm);
int PMPI_Reduce_scatter(void *, void *, int *, MPI_Datatype, MPI_Op, MPI_Comm);

/* 4.11 */

int  MPI_Scan(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);
int PMPI_Scan(void *, void *, int, MPI_Datatype, MPI_Op, MPI_Comm);

/* 5.3 */

int  MPI_Group_size(MPI_Group, int *);
int PMPI_Group_size(MPI_Group, int *);

int  MPI_Group_rank(MPI_Group, int *);
int PMPI_Group_rank(MPI_Group, int *);

int  MPI_Group_translate_ranks(MPI_Group, int, int *, MPI_Group, int *);
int PMPI_Group_translate_ranks(MPI_Group, int, int *, MPI_Group, int *);

int  MPI_Group_compare(MPI_Group, MPI_Group, int *);
int PMPI_Group_compare(MPI_Group, MPI_Group, int *);

int  MPI_Comm_group(MPI_Comm, MPI_Group *);
int PMPI_Comm_group(MPI_Comm, MPI_Group *);

int  MPI_Group_union(MPI_Group, MPI_Group, MPI_Group *);
int PMPI_Group_union(MPI_Group, MPI_Group, MPI_Group *);

int  MPI_Group_intersection(MPI_Group, MPI_Group, MPI_Group *);
int PMPI_Group_intersection(MPI_Group, MPI_Group, MPI_Group *);

int  MPI_Group_difference(MPI_Group, MPI_Group, MPI_Group *);
int PMPI_Group_difference(MPI_Group, MPI_Group, MPI_Group *);

int  MPI_Group_incl(MPI_Group, int, int *, MPI_Group *);
int PMPI_Group_incl(MPI_Group, int, int *, MPI_Group *);

int  MPI_Group_excl(MPI_Group, int, int *, MPI_Group *);
int PMPI_Group_excl(MPI_Group, int, int *, MPI_Group *);

int  MPI_Group_range_incl(MPI_Group, int, int [][3], MPI_Group *);
int PMPI_Group_range_incl(MPI_Group, int, int [][3], MPI_Group *);

int  MPI_Group_range_excl(MPI_Group, int, int [][3], MPI_Group *);
int PMPI_Group_range_excl(MPI_Group, int, int [][3], MPI_Group *);

int  MPI_Group_free(MPI_Group *);
int PMPI_Group_free(MPI_Group *);

/* 5.4 */

int  MPI_Comm_size(MPI_Comm, int *);
int PMPI_Comm_size(MPI_Comm, int *);

int  MPI_Comm_rank(MPI_Comm, int *);
int PMPI_Comm_rank(MPI_Comm, int *);

int  MPI_Comm_compare(MPI_Comm, MPI_Comm, int *);
int PMPI_Comm_compare(MPI_Comm, MPI_Comm, int *);

int  MPI_Comm_dup(MPI_Comm, MPI_Comm *);
int PMPI_Comm_dup(MPI_Comm, MPI_Comm *);

int  MPI_Comm_create(MPI_Comm, MPI_Group, MPI_Comm *);
int PMPI_Comm_create(MPI_Comm, MPI_Group, MPI_Comm *);

int  MPI_Comm_split(MPI_Comm, int, int, MPI_Comm *);
int PMPI_Comm_split(MPI_Comm, int, int, MPI_Comm *);

int  MPI_Comm_free(MPI_Comm *);
int PMPI_Comm_free(MPI_Comm *);

/* 5.6 */

int  MPI_Comm_test_inter(MPI_Comm, int *);
int PMPI_Comm_test_inter(MPI_Comm, int *);

int  MPI_Comm_remote_size(MPI_Comm, int *);
int PMPI_Comm_remote_size(MPI_Comm, int *);

int  MPI_Comm_remote_group(MPI_Comm, MPI_Group *);
int PMPI_Comm_remote_group(MPI_Comm, MPI_Group *);

int  MPI_Intercomm_create(MPI_Comm, int, MPI_Comm, int, int, MPI_Comm *);
int PMPI_Intercomm_create(MPI_Comm, int, MPI_Comm, int, int, MPI_Comm *);

int  MPI_Intercomm_merge(MPI_Comm, int, MPI_Comm *);
int PMPI_Intercomm_merge(MPI_Comm, int, MPI_Comm *);

/* 5.7 */

int  MPI_Keyval_create(MPI_Copy_function *, MPI_Delete_function *, int *, void *);
int PMPI_Keyval_create(MPI_Copy_function *, MPI_Delete_function *, int *, void *);

int  MPI_Keyval_free(int *);
int PMPI_Keyval_free(int *);

int  MPI_Attr_put(MPI_Comm, int, void *);
int PMPI_Attr_put(MPI_Comm, int, void *);

int  MPI_Attr_get(MPI_Comm, int, void *, int *);
int PMPI_Attr_get(MPI_Comm, int, void *, int *);

int  MPI_Attr_delete(MPI_Comm, int);
int PMPI_Attr_delete(MPI_Comm, int);

/* 6.5 */

int  MPI_Cart_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);
int PMPI_Cart_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);

int  MPI_Dims_create(int, int, int *);
int PMPI_Dims_create(int, int, int *);

int  MPI_Graph_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);
int PMPI_Graph_create(MPI_Comm, int, int *, int *, int, MPI_Comm *);

int  MPI_Topo_test(MPI_Comm, int *);
int PMPI_Topo_test(MPI_Comm, int *);

int  MPI_Graphdims_get(MPI_Comm, int *, int *);
int PMPI_Graphdims_get(MPI_Comm, int *, int *);

int  MPI_Graph_get(MPI_Comm, int, int, int *, int *);
int PMPI_Graph_get(MPI_Comm, int, int, int *, int *);

int  MPI_Cartdim_get(MPI_Comm, int *);
int PMPI_Cartdim_get(MPI_Comm, int *);

int  MPI_Cart_get(MPI_Comm, int, int *, int *, int *);
int PMPI_Cart_get(MPI_Comm, int, int *, int *, int *);

int  MPI_Cart_rank(MPI_Comm, int *, int *);
int PMPI_Cart_rank(MPI_Comm, int *, int *);

int  MPI_Cart_coords(MPI_Comm, int, int, int *);
int PMPI_Cart_coords(MPI_Comm, int, int, int *);

int  MPI_Graph_neighbors_count(MPI_Comm, int, int *);
int PMPI_Graph_neighbors_count(MPI_Comm, int, int *);

int  MPI_Graph_neighbors(MPI_Comm, int, int, int *);
int PMPI_Graph_neighbors(MPI_Comm, int, int, int *);

int  MPI_Cart_shift(MPI_Comm, int, int, int *, int *);
int PMPI_Cart_shift(MPI_Comm, int, int, int *, int *);

int  MPI_Cart_sub(MPI_Comm, int *, MPI_Comm *);
int PMPI_Cart_sub(MPI_Comm, int *, MPI_Comm *);

int  MPI_Cart_map(MPI_Comm, int, int *, int *, int *);
int PMPI_Cart_map(MPI_Comm, int, int *, int *, int *);

int  MPI_Graph_map(MPI_Comm, int, int *, int *, int *);
int PMPI_Graph_map(MPI_Comm, int, int *, int *, int *);

/* 7.1 */

int  MPI_Get_processor_name(char *, int *);
int PMPI_Get_processor_name(char *, int *);

/* 7.2 */

int  MPI_Errhandler_create(MPI_Handler_function *, MPI_Errhandler *);
int PMPI_Errhandler_create(MPI_Handler_function *, MPI_Errhandler *);

int  MPI_Errhandler_set(MPI_Comm, MPI_Errhandler);
int PMPI_Errhandler_set(MPI_Comm, MPI_Errhandler);

int  MPI_Errhandler_get(MPI_Comm, MPI_Errhandler *);
int PMPI_Errhandler_get(MPI_Comm, MPI_Errhandler *);

int  MPI_Errhandler_free(MPI_Errhandler *);
int PMPI_Errhandler_free(MPI_Errhandler *);

int  MPI_Error_string(int, char *, int *);
int PMPI_Error_string(int, char *, int *);

/* 7.3 */

int  MPI_Error_class(int, int *);
int PMPI_Error_class(int, int *);

/* 7.4 */

double  MPI_Wtime(void);
double PMPI_Wtime(void);

double  MPI_Wtick(void);
double PMPI_Wtick(void);

/* 7.5 */

int  MPI_Init(int *, char ***);
int PMPI_Init(int *, char ***);

int  MPI_Finalize(void);
int PMPI_Finalize(void);

int  MPI_Initialized(int *);
int PMPI_Initialized(int *);

int  MPI_Abort(MPI_Comm, int);
int PMPI_Abort(MPI_Comm, int);

/* 8.3 */

int  MPI_Pcontrol(int, ...);
int PMPI_Pcontrol(int, ...);

/********************/
/* MPI-1.2 bindings */
/********************/

#define MPI_VERSION		1
#define MPI_SUBVERSION		2

int  MPI_Get_version(int *, int *);
int PMPI_Get_version(int *, int *);


/*************************************/
/* MPI-2 bindings, sorted by chapter */
/*************************************/

/* 4.5.2 */

int  MPI_Request_get_status(MPI_Request, int *, MPI_Status *);
int PMPI_Request_get_status(MPI_Request, int *, MPI_Status *);


/* 4.10 */

typedef unsigned int          MPI_Info;

enum {
	MPI_INFO_NULL		= 0,
	MPI_MAX_INFO_KEY        = 255,
	MPI_MAX_INFO_VAL        = 1024
};

enum {
	MPI_FUNDAMENTAL		= -1
};

int  MPI_Info_create(MPI_Info *);
int  PMPI_Info_create(MPI_Info *);

int MPI_Info_delete(MPI_Info, char *);
int PMPI_Info_delete(MPI_Info, char *);

int MPI_Info_dup(MPI_Info, MPI_Info *);
int PMPI_Info_dup(MPI_Info, MPI_Info *);

int MPI_Info_free(MPI_Info *);
int PMPI_Info_free(MPI_Info *);

int MPI_Info_get(MPI_Info, char *, int, char *, int *);
int PMPI_Info_get(MPI_Info, char *, int, char *, int *);

int MPI_Info_get_nkeys(MPI_Info, int *);
int PMPI_Info_get_nkeys(MPI_Info, int *);

int MPI_Info_get_nthkey(MPI_Info, int, char *);
int PMPI_Info_get_nthkey(MPI_Info, int, char *);

int MPI_Info_get_valuelen(MPI_Info, char *, int *, int *);
int PMPI_Info_get_valuelen(MPI_Info, char *, int *, int *);

int MPI_Info_set(MPI_Info, char *, char *);
int PMPI_Info_set(MPI_Info, char *, char *);

/* 4.11 */

int MPI_Alloc_mem(MPI_Aint,MPI_Info,void *);
int PMPI_Alloc_mem(MPI_Aint,MPI_Info,void *);

int MPI_Free_mem(void *);
int PMPI_Free_mem(void *);

/* 4.12 */

typedef int MPI_Fint;

MPI_Fint MPI_Info_c2f(MPI_Info);
MPI_Fint PMPI_Info_c2f(MPI_Info);

MPI_Info MPI_Info_f2c(MPI_Fint);
MPI_Info PMPI_Info_f2c(MPI_Fint);

MPI_Fint MPI_Comm_c2f(MPI_Comm);
MPI_Fint PMPI_Comm_c2f(MPI_Comm);

MPI_Comm MPI_Comm_f2c(MPI_Fint);
MPI_Comm PMPI_Comm_f2c(MPI_Fint);

MPI_Fint MPI_Type_c2f(MPI_Datatype);
MPI_Fint PMPI_Type_c2f(MPI_Datatype);

MPI_Datatype MPI_Type_f2c(MPI_Fint);
MPI_Datatype PMPI_Type_f2c(MPI_Fint);

MPI_Fint MPI_Group_c2f(MPI_Group);
MPI_Fint PMPI_Group_c2f(MPI_Group);

MPI_Group MPI_Group_f2c(MPI_Fint);
MPI_Group PMPI_Group_f2c(MPI_Fint);

MPI_Fint MPI_Request_c2f(MPI_Request);
MPI_Fint PMPI_Request_c2f(MPI_Request);

MPI_Request MPI_Request_f2c(MPI_Fint);
MPI_Request PMPI_Request_f2c(MPI_Fint);

MPI_Fint MPI_Op_c2f(MPI_Op);
MPI_Fint PMPI_Op_c2f(MPI_Op);

MPI_Op MPI_Op_f2c(MPI_Fint);
MPI_Op PMPI_Op_f2c(MPI_Fint);


/* 4.14 */

int  MPI_Type_create_hvector(int, int, MPI_Aint, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_create_hvector(int, int, MPI_Aint, MPI_Datatype, MPI_Datatype *);

int  MPI_Type_create_hindexed(int, int *, MPI_Aint *, MPI_Datatype, MPI_Datatype *);
int PMPI_Type_create_hindexed(int, int *, MPI_Aint *, MPI_Datatype, MPI_Datatype *);

int  MPI_Type_create_struct(int, int *, MPI_Aint *, MPI_Datatype *, MPI_Datatype *);
int PMPI_Type_create_struct(int, int *, MPI_Aint *, MPI_Datatype *, MPI_Datatype *);

int MPI_Get_address(void *, MPI_Aint *);
int PMPI_Get_address(void *, MPI_Aint *);

/* 5.3 */

#define MPI_ARGV_NULL              ((char **)NULL)
#define MPI_ARGVS_NULL              ((char ***)NULL)
#define MPI_ERRCODES_IGNORE        ((int *)NULL)

int MPI_Comm_spawn(char *, char **, int, MPI_Info, int, MPI_Comm, MPI_Comm *, int *);
int PMPI_Comm_spawn(char *, char **, int, MPI_Info, int, MPI_Comm, MPI_Comm *, int *);
int MPI_Comm_spawn_multiple(int , char **, char ***, int *, MPI_Info *,
                            int , MPI_Comm, MPI_Comm *, int *); 
int PMPI_Comm_spawn_multiple(int , char **, char ***, int *, MPI_Info *,
                            int , MPI_Comm, MPI_Comm *, int *); 
int MPI_Comm_get_parent(MPI_Comm *);
int PMPI_Comm_get_parent(MPI_Comm *);

/* 6 */

/* MPI one-sided is supported only under ABI 64.  */
#if 	!_ABIN32

enum {
	MPI_WIN_NULL 	= 0
};

/* referenced in 4.12 of MPI-2 standard with other transfer of handles functions */

MPI_Fint MPI_Win_c2f(MPI_Win);
MPI_Fint PMPI_Win_c2f(MPI_Win);

MPI_Win MPI_Win_f2c(MPI_Fint);
MPI_Win PMPI_Win_f2c(MPI_Fint);

/* 6.2 */

int  MPI_Win_create(void *, MPI_Aint, int, MPI_Info, MPI_Comm, MPI_Win *);
int PMPI_Win_create(void *, MPI_Aint, int, MPI_Info, MPI_Comm, MPI_Win *);

int  MPI_Win_fence(int, MPI_Win);
int PMPI_Win_fence(int, MPI_Win);

int  MPI_Win_free(MPI_Win *);
int PMPI_Win_free(MPI_Win *);

int  MPI_Win_lock(int, int, int, MPI_Win);
int PMPI_Win_lock(int, int, int, MPI_Win);
int  MPI_Win_unlock(int, MPI_Win);
int PMPI_Win_unlock(int, MPI_Win);

/* 6.3 */


int  MPI_Put(void *, int, MPI_Datatype, int, MPI_Aint, int, MPI_Datatype,
	MPI_Win);
int PMPI_Put(void *, int, MPI_Datatype, int, MPI_Aint, int, MPI_Datatype,
	MPI_Win);

int  MPI_Get(void *, int, MPI_Datatype, int, MPI_Aint, int, MPI_Datatype,
	MPI_Win);
int PMPI_Get(void *, int, MPI_Datatype, int, MPI_Aint, int, MPI_Datatype,
	MPI_Win);

int  MPI_Accumulate(void *, int, MPI_Datatype, int, MPI_Aint, int,
	MPI_Datatype, MPI_Op, MPI_Win);
int PMPI_Accumulate(void *, int, MPI_Datatype, int, MPI_Aint, int,
	MPI_Datatype, MPI_Op, MPI_Win);

/* 6.4 */

enum {
	MPI_MODE_NOCHECK	= 1,
	MPI_MODE_NOSTORE	= 2,
	MPI_MODE_NOPUT		= 4,
	MPI_MODE_NOPRECEDE	= 8,
	MPI_MODE_NOSUCCEED 	= 16
};


#endif	/* !_ABIN32 */


/* 7.5 */

int  MPI_Type_get_envelope(MPI_Datatype, int *, int *, int *, int *);
int PMPI_Type_get_envelope(MPI_Datatype, int *, int *, int *, int *);

int  MPI_Type_get_contents(MPI_Datatype, int, int, int, int *, MPI_Aint *, MPI_Datatype *);
int PMPI_Type_get_contents(MPI_Datatype, int, int, int, int *, MPI_Aint *, MPI_Datatype *);

/* 7.8 */

int  MPI_Type_dup(MPI_Datatype, MPI_Datatype *);
int PMPI_Type_dup(MPI_Datatype, MPI_Datatype *);

/* 8.2 */


typedef int MPI_Grequest_query_function(void *extra_state, MPI_Status *status);
typedef int MPI_Grequest_free_function(void *extra_state);
typedef int MPI_Grequest_cancel_function(void *extra_state, int complete);

int  MPI_Grequest_complete(MPI_Request *);
int PMPI_Grequest_complete(MPI_Request *);
 
int  MPI_Grequest_start(MPI_Grequest_query_function, MPI_Grequest_free_function, MPI_Grequest_cancel_function, void *, MPI_Request *);
int PMPI_Grequest_start(MPI_Grequest_query_function, MPI_Grequest_free_function, MPI_Grequest_cancel_function, void *, MPI_Request *);
 
int  MPI_Status_set_cancelled(MPI_Status *, int);
int PMPI_Status_set_cancelled(MPI_Status *, int);
 
int  MPI_Status_set_elements(MPI_Status *, MPI_Datatype, int);
int PMPI_Status_set_elements(MPI_Status *, MPI_Datatype, int);

/* 8.6 */

enum {
	MPI_COMBINER_NAMED      	= (-1),
	MPI_COMBINER_CONTIGUOUS 	= 0,
	MPI_COMBINER_VECTOR     	= 1,
	MPI_COMBINER_HVECTOR    	= 2,
	MPI_COMBINER_INDEXED    	= 3,
	MPI_COMBINER_HINDEXED   	= 4,
	MPI_COMBINER_STRUCT     	= 5,
	MPI_COMBINER_DARRAY		= 6,
	MPI_COMBINER_DUP		= 7,
	MPI_COMBINER_F90_COMPLES	= 8,
	MPI_COMBINER_F90_INTEGER	= 9,
	MPI_COMBINER_F90_REAL		= 10,
	MPI_COMBINER_HINDEXED_INTEGER	= 11,
	MPI_COMBINER_HVECTOR_INTEGER	= 12,
	MPI_COMBINER_INDEXED_BLOCK	= 13,
	MPI_COMBINER_RESIZED		= 14,
	MPI_COMBINER_STRUCT_INTEGER	= 15,
	MPI_COMBINER_SUBARRAY		= 16
};

/* 8.7 */

int  MPI_Init_thread(int *, char ***, int, int *);
int PMPI_Init_thread(int *, char ***, int, int *);
int  MPI_Query_thread(int *);
int PMPI_Query_thread(int *);
int  MPI_Is_thread_main(int *);
int PMPI_Is_thread_main(int *);

enum {
	MPI_THREAD_SINGLE		= 0,
	MPI_THREAD_FUNNELED		= 1,
	MPI_THREAD_SERIALIZED		= 2,
	MPI_THREAD_MULTIPLE		= 3
};

/* 8.8 */


int   MPI_Comm_create_keyval(MPI_Comm_copy_attr_function *, MPI_Comm_delete_attr_function *, 
                          int *, void *);
int  PMPI_Comm_create_keyval(MPI_Comm_copy_attr_function *, MPI_Comm_delete_attr_function *, 
                          int *, void *);

int  MPI_Comm_free_keyval(int *);
int PMPI_Comm_free_keyval(int *);

int  MPI_Comm_set_attr(MPI_Comm, int, void *);
int PMPI_Comm_set_attr(MPI_Comm, int, void *);

int  MPI_Comm_get_attr(MPI_Comm, int, void *, int *);
int PMPI_Comm_get_attr(MPI_Comm, int, void *, int *);

int  MPI_Comm_delete_attr(MPI_Comm, int);
int PMPI_Comm_delete_attr(MPI_Comm, int);

/* MPI one-sided is supported only under ABI 64.  */
#if 	!_ABIN32

enum {
        MPI_WIN_BASE            = 1,
        MPI_WIN_SIZE            = 2,
        MPI_WIN_DISP_UNIT       = 3
};

int   MPI_Win_create_keyval(MPI_Win_copy_attr_function *, MPI_Win_delete_attr_function *,
                          int *, void *);
int  PMPI_Win_create_keyval(MPI_Win_copy_attr_function *, MPI_Win_delete_attr_function *,
                          int *, void *);

int  MPI_Win_free_keyval(int *);
int PMPI_Win_free_keyval(int *);

int  MPI_Win_set_attr(MPI_Win, int, void *);
int PMPI_Win_set_attr(MPI_Win, int, void *);

int  MPI_Win_get_attr(MPI_Win, int, void *, int *);
int PMPI_Win_get_attr(MPI_Win, int, void *, int *);

int  MPI_Win_delete_attr(MPI_Win, int);
int PMPI_Win_delete_attr(MPI_Win, int);

#endif	/* !_ABIN32 */

int   MPI_Type_create_keyval(MPI_Type_copy_attr_function *, MPI_Type_delete_attr_function *,
                          int *, void *);
int  PMPI_Type_create_keyval(MPI_Type_copy_attr_function *, MPI_Type_delete_attr_function *,
                          int *, void *);

int  MPI_Type_free_keyval(int *);
int PMPI_Type_free_keyval(int *);

int  MPI_Type_set_attr(MPI_Datatype, int, void *);
int PMPI_Type_set_attr(MPI_Datatype, int, void *);

int  MPI_Type_get_attr(MPI_Datatype, int, void *, int *);
int PMPI_Type_get_attr(MPI_Datatype, int, void *, int *);

int  MPI_Type_delete_attr(MPI_Datatype, int);
int PMPI_Type_delete_attr(MPI_Datatype, int);


/* 9.6 */

int  MPI_Finalized(int *);
int PMPI_Finalized(int *);

#if defined(__cplusplus)
}
#if !defined(MPI_NO_CPPBIND)
#include "mpi++.h"
#endif
#endif

/*
 * mpio.h must be included after mpi.h 
 */
#include "mpio.h"

#endif	/* MPI_H_INCLUDED */
