/* $Id: mpi++.h,v 1.7 2002/03/28 16:40:24 tmiller Exp $ */
// -*- c++ -*-
//
// Copyright 1997-1999, University of Notre Dame.
// Authors:  Jeremy G. Siek, Michael P. McNally, Jeffery M. Squyres, 
//           Andrew Lumsdaine
//
// This file is part of the Notre Dame C++ bindings for MPI
//
// You should have received a copy of the License Agreement for the
// Notre Dame C++ bindings for MPI along with the software;  see the
// file LICENSE.mpi++.  If not, contact Office of Research, University of Notre
// Dame, Notre Dame, IN  46556.
//
// Permission to modify the code and to distribute modified code is
// granted, provided the text of this NOTICE is retained, a notice that
// the code was modified is included with the above COPYRIGHT NOTICE and
// with the COPYRIGHT NOTICE in the LICENSE file, and that the LICENSE
// file is distributed with the modified code.
//
// LICENSOR MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.
// By way of example, but not limitation, Licensor MAKES NO
// REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
// PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED SOFTWARE COMPONENTS
// OR DOCUMENTATION WILL NOT INFRINGE ANY PATENTS, COPYRIGHTS, TRADEMARKS
// OR OTHER RIGHTS.
//

#ifndef MPIPP_H
#define MPIPP_H

/* process-header: including mpi2c++_config.h */

#ifndef _MPIPP_CONFIG_H
#define _MPIPP_CONFIG_H

#define _MPIPP_USEEXCEPTIONS_ 1
#define _MPIPP_DEBUG_         0

#define MPI2CPP_HAVE_BOOL     1 

#define MPI_SGI_SIZEOF_INT            4
#define SIZEOF_MPI2CPP_BOOL_T 1

#if MPI_SGI_SIZEOF_INT != SIZEOF_MPI2CPP_BOOL_T
#define _MPIPP_BOOL_NE_INT_   1
#else
#define _MPIPP_BOOL_NE_INT_   0
#endif

// Does our compiler support namespaces?
#define _MPIPP_USENAMESPACE_  1

// Compile for Profiling?
#define _MPIPP_PROFILING_     1

// What kind of signals do we have?
#define MPI2CPP_BSD_SIGNAL    0
#define MPI2CPP_SYSV_SIGNAL   1

// Is the ERR_PENDING constant defined?
#define MPI2CPP_HAVE_PENDING  0

// Can virtual functions return derived class instead
// of base class?
#define VIRTUAL_FUNC_RET      0

// For mpich, tell it to use correct MPI_Handler_function definition
#define USE_STDARG

#if _MPIPP_PROFILING_
#define _REAL_MPI_ PMPI
#else
#define _REAL_MPI_ MPI
#endif

#if _MPIPP_USENAMESPACE_
#define _MPIPP_STATIC_
#define _MPIPP_EXTERN_ extern
#else
#define _MPIPP_STATIC_ static
#define _MPIPP_EXTERN_
#endif

#if MPI2CPP_HAVE_BOOL
typedef bool MPI2CPP_BOOL_T;
#define MPI2CPP_FALSE false
#define MPI2CPP_TRUE true
#else
enum MPI2CPP_BOOL_T { MPI2CPP_FALSE, MPI2CPP_TRUE };
#ifdef bool
/* 1 line(s) elided by process-header */
#endif
#ifdef false
/* 1 line(s) elided by process-header */
#endif
#ifdef true
/* 1 line(s) elided by process-header */
#endif
#define bool MPI2CPP_BOOL_T
#define false MPI2CPP_FALSE
#define true MPI2CPP_TRUE
#endif

#define MPI2CPP_BSD_SIGNAL 0
#define MPI2CPP_SYSV_SIGNAL 1

// Flags for FORTRAN, optional C, and optional Fortran datatypes

#define MPI2CPP_FORTRAN 1
#define MPI2CPP_ALL_OPTIONAL_FORTRAN 0
#define MPI2CPP_SOME_OPTIONAL_FORTRAN 1
#define MPI2CPP_OPTIONAL_C 0

//
// Architecture/OS's that will need flags in the test suite to
// ignore certain tests
//
 
#define MPI2CPP_LAM61 0
#define MPI2CPP_LAM62 0
#define MPI2CPP_LAM63 0
#define MPI2CPP_LAMUNKNOWN 0

#define MPI2CPP_MPICH1013 0
#define MPI2CPP_MPICH110 0
#define MPI2CPP_MPICH111 0
#define MPI2CPP_MPICH112 0
#define MPI2CPP_MPICHUNKNOWN 0
 
#define MPI2CPP_IBM21014 0
#define MPI2CPP_IBM21015 0
#define MPI2CPP_IBM21016 0
#define MPI2CPP_IBM21017 0
#define MPI2CPP_IBM21018 0
#define MPI2CPP_IBMUNKNOWN 0
#define MPI2CPP_IBM_SP (MPI2CPP_IBM21014 | MPI2CPP_IBM21015 | MPI2CPP_IBM21016 | MPI2CPP_IBM21017 | MPI2CPP_IBM21018 | MPI2CPP_IBMUNKNOWN)
 
#define MPI2CPP_SGI20 0
#define MPI2CPP_SGI30 0
#define MPI2CPP_SGIUNKNOWN 1

#define MPI2CPP_HPUX0102 0
#define MPI2CPP_HPUX0103 0
#define MPI2CPP_HPUX0105 0
#define MPI2CPP_HPUXUNKNOWN 0

#define MPI2CPP_CRAY 0
#define MPI2CPP_CRAY1104 0
#define MPI2CPP_CRAYUNKNWON 0

#define MPI2CPP_G_PLUS_PLUS 0

#define MPI2CPP_ATTR long

// #$%@#%@#%@#% AIX!!!!

#define MPI2CPP_AIX 0

#endif
/* process-header: end of mpi2c++_config.h */

extern "C" {
#include <mpi.h>
}

#if defined(_STANDARD_C_PLUS_PLUS)
#include <iostream>
#else
#include <iostream.h>
#endif
#include <stdarg.h>
/* process-header: including mpi2c++_map.h */


#ifndef MAP_H_
#define MAP_H_

/* process-header: including mpi2c++_list.h */


#ifndef LIST_H_
#define LIST_H_

/* process-header: including mpi2c++_config.h */

#ifndef _MPIPP_CONFIG_H
/* 111 line(s) elided by process-header */
#endif
/* process-header: end of mpi2c++_config.h */

class MPI_SGI_List {
public:
  typedef void* Data;
  class iter;

  class Link {
    friend class MPI_SGI_List;
    friend class iter;
    Data data;
    Link *next;
    Link *prev;
    Link() { }
    Link(Data d, Link* p, Link* n) : data(d), next(n), prev(p) { }
  };

  class iter {
    friend class MPI_SGI_List;
    Link* node;
  public:
    iter(Link* n) : node(n) { }
    iter& operator++() { node = node->next; return *this; }
    iter operator++(int) { iter tmp = *this; ++(*this); return tmp; }
    Data& operator*() const { return node->data; }
    MPI2CPP_BOOL_T operator==(const iter& x) const { return (MPI2CPP_BOOL_T)(node == x.node); }
    MPI2CPP_BOOL_T operator!=(const iter& x) const { return (MPI2CPP_BOOL_T)(node != x.node); }
  };
  
  MPI_SGI_List() { _end.prev = &_end; _end.next = &_end; }
  virtual ~MPI_SGI_List() {
    for (iter i = begin(); i != end(); ) {
      Link* garbage = i.node; i++;
      delete garbage;
    }
  }
  virtual iter begin() { return _end.next; }
  virtual iter end() { return &_end; }
  virtual iter insert(iter p, Data d) {
    iter pos(p);
    Link* n = new Link(d, pos.node->prev, pos.node);
    pos.node->prev->next = n;
    pos.node->prev = n;
    return n;
  }
  void erase(iter pos) {
    pos.node->prev->next = pos.node->next;
    pos.node->next->prev = pos.node->prev;
    delete pos.node;
  }

protected:
  Link _end;
};

#endif




/* process-header: end of mpi2c++_list.h */
typedef MPI_SGI_List Container;

class MPI_SGI_Map {
  Container c;
public:

  typedef void* address;
  typedef MPI_SGI_List::iter iter;

  struct Pair {
    Pair(address f, address s) : first(f), second(s) {}
    Pair() : first(0), second(0) { }
    address first;
    address second;
  };

  MPI_SGI_Map() { }

  ~MPI_SGI_Map() {
    for (iter i = c.end(); i != c.end(); i++) {
      delete (Pair*)(*i);
    }
  }
  
  Pair* begin();
  Pair* end();
  
  address& operator[](address key)
  {
    address* found = (address*)0;
    for (iter i = c.begin(); i != c.end(); i++) {
      if (((Pair*)*i)->first == key)
	found = &((Pair*)*i)->second;
    }
    if (! found) {
      iter tmp = c.insert(c.begin(), new Pair(key,0));
      found = &((Pair*)*tmp)->second;
    }
    return *found;
  }

  void erase(address key)
  {
    for (iter i = c.begin(); i != c.end(); i++) {
      if (((Pair*)*i)->first == key) {
	delete (Pair*)*i;
	c.erase(i); break;
      }
    }
  }
};

#endif




/* process-header: end of mpi2c++_map.h */



//JGS: this is used for implementing user functions for MPI::Op
extern void
op_intercept(void *invec, void *outvec, int *len, MPI_Datatype *datatype);

#if MPI2CPP_IBM_SP
//Here's the sp2 typedeffrom their header file:
//  typedef void MPI_Handler_function(MPI_Comm *,int *,char *,int *,int *);
extern void
errhandler_intercept(MPI_Comm * mpi_comm, int * err, char*, int*, int*);

extern void
throw_excptn_fctn(MPI_Comm* comm, int* errcode, char*, int*, int*);


#else

//JGS: this is used as the MPI_Handler_function for
// the mpi_errhandler in ERRORS_THROW_EXCEPTIONS
extern void
throw_excptn_fctn(MPI_Comm* comm, int* errcode, ...);

extern void
errhandler_intercept(MPI_Comm * mpi_comm, int * err, ...);
#endif


//used for attr intercept functions
enum CommType { eIntracomm, eIntercomm, eCartcomm, eGraphcomm};

extern int
copy_attr_intercept(MPI_Comm oldcomm, int keyval, 
		    void *extra_state, void *attribute_val_in, 
		    void *attribute_val_out, int *flag);

extern int
delete_attr_intercept(MPI_Comm comm, int keyval, 
		      void *attribute_val, void *extra_state);


#if _MPIPP_PROFILING_
/* process-header: including pmpi++.h */


#ifndef PMPIPP_H
#define PMPIPP_H


#if _MPIPP_USENAMESPACE_
namespace PMPI {
#else
class PMPI {
public:
#endif


#if ! _MPIPP_USEEXCEPTIONS_
  _MPIPP_EXTERN_ _MPIPP_STATIC_ int errno;
#endif

  class Comm_Null;
  class Comm;
  class Intracomm;
  class Intercomm;
  class Graphcomm;
  class Cartcomm;
  class Datatype;
  class Errhandler;
  class Group;
  class Op;
  class Request;
  class Status;

  typedef MPI_Aint Aint;

/* process-header: including functions.h */

//
// Point-to-Point Communication
//

_MPIPP_STATIC_ void 
Attach_buffer(void* buffer, int size);

_MPIPP_STATIC_ int 
Detach_buffer(void*& buffer);

//
// Process Topologies
//

_MPIPP_STATIC_ void
Compute_dims(int nnodes, int ndims, int dims[]);

//
// Environmental Inquiry
//

_MPIPP_STATIC_ void 
Get_processor_name(char*& name, int& resultlen);

_MPIPP_STATIC_ void
Get_error_string(int errorcode, char*& string, int& resultlen);

_MPIPP_STATIC_ int 
Get_error_class(int errorcode);

_MPIPP_STATIC_ double 
Wtime();

_MPIPP_STATIC_ double 
Wtick();

_MPIPP_STATIC_ void
Init(int& argc, char**& argv);

_MPIPP_STATIC_ void
Init();

_MPIPP_STATIC_ void
Real_init();

_MPIPP_STATIC_ void
Finalize();

_MPIPP_STATIC_ MPI2CPP_BOOL_T
Is_initialized();

//
// Profiling
//

_MPIPP_STATIC_ void
Pcontrol(const int level, ...);


// JGS defer to MPI-2
//inline _MPIPP_STATIC_ void
//Get_version(int& version, int& subversion);

_MPIPP_STATIC_ _REAL_MPI_::Aint
Get_address(void* location);
/* process-header: end of functions.h */
/* process-header: including pdatatype.h */


class Datatype {
public:
  // construction / destruction
  inline Datatype() : mpi_datatype(MPI_DATATYPE_NULL) { }
  inline virtual ~Datatype() {}
  // inter-language operability
  inline Datatype(const MPI_Datatype &i) : mpi_datatype(i) { }

  // copy / assignment
  Datatype(const Datatype& dt) : mpi_datatype(dt.mpi_datatype) { }

  Datatype& operator=(const Datatype& dt) {
    mpi_datatype = dt.mpi_datatype; return *this;
  }
  
  // comparison
  MPI2CPP_BOOL_T operator==(const Datatype &a) const {
    return (MPI2CPP_BOOL_T) (mpi_datatype == a.mpi_datatype);
  }

  inline MPI2CPP_BOOL_T operator!= (const Datatype &a) const 
    { return (MPI2CPP_BOOL_T) !(*this == a); }

  // inter-language operability
  inline Datatype& operator= (const MPI_Datatype &i) 
    { mpi_datatype = i; return *this; }

  inline operator MPI_Datatype () const { return mpi_datatype; }

  inline const MPI_Datatype& mpi() const { return mpi_datatype; }

  //
  // Point-to-Point Communication
  //

  inline virtual Datatype Create_contiguous(int count) const;

  virtual Datatype Create_vector(int count, int blocklength,
					int stride) const;

  virtual Datatype Create_indexed(int count,
				       const int array_of_blocklengths[], 
				       const int array_of_displacements[]) const;
  
  static Datatype Create_struct(int count, const int array_of_blocklengths[],
				     const Aint array_of_displacements[],
				     const Datatype array_if_types[]);
  

  virtual Datatype Create_hindexed(int count, const int array_of_blocklengths[],
					const Aint array_of_displacements[]) const;

  virtual Datatype Create_hvector(int count, int blocklength,
				       Aint stride) const;

  virtual int Get_size() const;

  virtual void Get_extent(Aint& lb, Aint& extent) const;

  virtual void Commit();

  virtual void Free();

  //JGS I did not make these inline becuase of the dependency
  //on the Comm conversion function
  virtual void Pack(const void* inbuf, int incount, void *outbuf, 
		    int outsize, int& position, const Comm &comm) const;
  virtual void Unpack(const void* inbuf, int insize, void *outbuf,
		      int outcount, int& position, const Comm& comm) const;
  virtual int Pack_size(int incount, const Comm& comm) const;

protected:
  MPI_Datatype mpi_datatype;

};
/* process-header: end of pdatatype.h */

  typedef void User_function(const void* invec, void* inoutvec, int len,
			     const Datatype& datatype);

/* process-header: including pexception.h */


class Exception {
public:
  inline Exception(int ec) : error_code(ec) {
    (void)MPI_Error_class(error_code, &error_class);
    int resultlen;
    error_string = new char[MPI_MAX_ERROR_STRING];
    (void)MPI_Error_string(error_code, error_string, &resultlen);
  }

  inline int Get_error_code() const { return error_code; }

  inline int Get_error_class() const { return error_class; }
  
  inline const char* Get_error_string() const { return error_string; }
  
protected:
  int error_code;
  char* error_string;
  int error_class;
};
/* process-header: end of pexception.h */
/* process-header: including pop.h */


class Op {
public:
  // construction
  Op() : mpi_op(MPI_OP_NULL) { }

  Op(const Op& op) : mpi_op(op.mpi_op), op_user_function(op.op_user_function) { }

  Op(const MPI_Op &i) : mpi_op(i) { }

  // destruction
  virtual ~Op() { }

  // assignment
  Op& operator=(const Op& op);
  Op& operator= (const MPI_Op &i);
  // comparison
  MPI2CPP_BOOL_T operator== (const Op &a);
  MPI2CPP_BOOL_T operator!= (const Op &a);
  // conversion functions for inter-language operability
  operator MPI_Op () const;
  //  operator MPI_Op* () const;

  // Collective Communication  (defined in op_inln.h)
  virtual void Init(User_function *func, MPI2CPP_BOOL_T commute);
  virtual void Free(void);

  User_function *op_user_function;

protected:
  MPI_Op mpi_op;

};

/* process-header: end of pop.h */
/* process-header: including pstatus.h */


class Status {
  friend class PMPI::Comm; //so I can access pmpi_status data member in comm.cc
  friend class PMPI::Request; //and also from request.cc

public:
  // construction
  inline Status() { }
  // copy
  Status(const Status& data) : mpi_status(data.mpi_status) { }

  inline Status(const MPI_Status &i) : mpi_status(i) { }

  inline virtual ~Status() {}

  Status& operator=(const Status& data) {
    mpi_status = data.mpi_status; return *this;
  } 

  // comparison, don't need for status

  // inter-language operability
  inline Status& operator= (const MPI_Status &i) {
    mpi_status = i; return *this; }
  inline operator MPI_Status () const { return mpi_status; }
  inline operator MPI_Status* () const { return (MPI_Status*)&mpi_status; }

  //
  // Point-to-Point Communication
  //

  inline virtual int Get_count(const Datatype& datatype) const;

  inline virtual MPI2CPP_BOOL_T Is_cancelled() const;

  virtual int Get_elements(const Datatype& datatype) const;

  //
  // Status Access
  //
  inline virtual int Get_source() const;

  inline virtual void Set_source(int source);
  
  inline virtual int Get_tag() const;

  inline virtual void Set_tag(int tag);
  
  inline virtual int Get_error() const;

  inline virtual void Set_error(int error);
  
protected:
  MPI_Status mpi_status;

};
/* process-header: end of pstatus.h */
/* process-header: including prequest.h */


class Request {
public:
  // construction / destruction
  Request(void) { }
  virtual ~Request() {}
  Request(const MPI_Request &i) : mpi_request(i) { }

  // copy / assignment
  Request(const Request& r) : mpi_request(r.mpi_request) { }

  Request& operator=(const Request& r) {
    mpi_request = r.mpi_request;
    return *this;
  }

  // comparison
  MPI2CPP_BOOL_T operator== (const Request &a)
  { return (MPI2CPP_BOOL_T)(mpi_request == a.mpi_request); }
  MPI2CPP_BOOL_T operator!= (const Request &a) 
  { return (MPI2CPP_BOOL_T)!(*this == a); }

  // inter-language operability
  Request& operator= (const MPI_Request &i) {
    mpi_request = i; return *this; }
  operator MPI_Request () const { return mpi_request; }
  operator MPI_Request* () const { return (MPI_Request*)&mpi_request; }

  //
  // Point-to-Point Communication
  //

  virtual void Wait(Status &status);

  virtual void Wait();

  virtual MPI2CPP_BOOL_T Test(Status &status);

  virtual MPI2CPP_BOOL_T Test();

  virtual void Free(void);

  static int Waitany(int count, Request array[], Status& status);

  static int Waitany(int count, Request array[]);

  static MPI2CPP_BOOL_T Testany(int count, Request array[], int& index, Status& status);

  static MPI2CPP_BOOL_T Testany(int count, Request array[], int& index);

  static void Waitall(int count, Request req_array[], Status stat_array[]);
 
  static void Waitall(int count, Request req_array[]);

  static MPI2CPP_BOOL_T Testall(int count, Request req_array[], Status stat_array[]);
 
  static MPI2CPP_BOOL_T Testall(int count, Request req_array[]);

  static int Waitsome(int incount, Request req_array[],
			     int array_of_indices[], Status stat_array[]);

  static int Waitsome(int incount, Request req_array[],
			     int array_of_indices[]);

  static int Testsome(int incount, Request req_array[],
			     int array_of_indices[], Status stat_array[]);

  static int Testsome(int incount, Request req_array[],
			     int array_of_indices[]);

  virtual void Cancel(void) const;

protected:
  MPI_Request mpi_request;

private:
  static Status ignored_status;
  
};


class Prequest : public Request {
public:

  Prequest() { }

  Prequest(const Prequest& p) : Request(p) { }
  
  Prequest(const MPI_Request &i) : Request(i) { }

  virtual ~Prequest() { }

  virtual void Start();

  static void Startall(int count, Prequest array_of_requests[]);

};
/* process-header: end of prequest.h */
/* process-header: including pgroup.h */

class Group {
public:
  // construction / destruction
  inline Group() : mpi_group(MPI_GROUP_NULL) { }
  inline virtual ~Group() {}
  inline Group(const MPI_Group &i) : mpi_group(i) { }

  // copy / assignment
  Group(const Group& g): mpi_group(g.mpi_group) { }

  Group& operator=(const Group& g) {
    mpi_group = g.mpi_group;
    return *this;
  }

  // comparison
  MPI2CPP_BOOL_T operator== (const Group &a) {
    return (MPI2CPP_BOOL_T)(mpi_group == a.mpi_group);
  }
  MPI2CPP_BOOL_T operator!= (const Group &a) { return (MPI2CPP_BOOL_T)!(*this == a); }
 
  // inter-language operability
  inline Group& operator= (const MPI_Group &i) { mpi_group = i; return *this; }
  inline operator const MPI_Group& () const { return mpi_group; }
  inline operator MPI_Group* () const { return (MPI_Group*)&mpi_group; }

  inline const MPI_Group& mpi() const { return mpi_group; }
  //
  // Groups, Contexts, and Communicators
  //

  virtual int Get_size() const;
  
  virtual int Get_rank() const;

  static void Translate_ranks(const Group& group1, int n, const int ranks1[], 
			      const Group& group2, int ranks2[]);
  
  static int Compare(const Group& group1, const Group& group2);

  static Group Union(const Group &group1, const Group &group2);

  static Group Intersect(const Group &group1, const Group &group2);

  static Group Difference(const Group &group1, const Group &group2);

  virtual Group Incl(int n, const int ranks[]) const;

  virtual Group Excl(int n, const int ranks[]) const;

  virtual Group Range_incl(int n, const int ranges[][3]) const;

  virtual Group Range_excl(int n, const int ranges[][3]) const;

  virtual void Free();

protected:
  MPI_Group mpi_group;

};

/* process-header: end of pgroup.h */
/* process-header: including pcomm.h */

class Comm_Null {
public:

  // construction
  Comm_Null() : mpi_comm(MPI_COMM_NULL) { }
  // copy
  Comm_Null(const Comm_Null& data) : mpi_comm(data.mpi_comm) { }
  // inter-language operability  
  Comm_Null(const MPI_Comm& data) : mpi_comm(data) { }

  // destruction
  //JGS  virtual ~Comm_Null() { }

 // comparison
  // JGS make sure this is right (in other classes too)
  MPI2CPP_BOOL_T operator==(const Comm_Null& data) const {
    return (MPI2CPP_BOOL_T) (mpi_comm == data.mpi_comm); }

  MPI2CPP_BOOL_T operator!=(const Comm_Null& data) const {
    return (MPI2CPP_BOOL_T) !(*this == data);}

  // inter-language operability (conversion operators)
  operator MPI_Comm() const { return mpi_comm; }
  operator MPI_Comm*() /*const JGS*/ { return &mpi_comm; }

protected:
  MPI_Comm mpi_comm;
};

class Comm : public Comm_Null {

#if 0 // JGS getting rid of !@(&!$@ friends, justing making some stuff public
#if _MPIPP_USENAMESPACE_
#if IBM_SP
  friend void ::errhandler_intercept(MPI_Comm * mpi_comm, int * err, char*, int*, int*);

#else
  friend void ::errhandler_intercept(MPI_Comm * mpi_comm, int* err, ...);
#endif
  friend int ::copy_attr_intercept(MPI_Comm oldcomm, int keyval, 
				   void *extra_state, void *attribute_val_in, 
				   void *attribute_val_out, int *flag);
  friend int ::delete_attr_intercept(MPI_Comm comm, int keyval, 
				     void *attribute_val, void *extra_state);
#endif

#if ! _MPIPP_USENAMESPACE_

#if IBM_SP
  friend void errhandler_intercept(MPI_Comm * mpi_comm, int * err, char*, int*, int*);
#else
  friend void errhandler_intercept(MPI_Comm * mpi_comm, int* err, ...);
#endif
  friend int copy_attr_intercept(MPI_Comm oldcomm, int keyval, 
				 void *extra_state, void *attribute_val_in, 
				 void *attribute_val_out, int *flag);
  friend int delete_attr_intercept(MPI_Comm comm, int keyval, 
				   void *attribute_val, void *extra_state);
#endif
#endif

public:

  // $%^#$%^#$^ g++ 2.7.2!!! We have to rename these functions because
  // g++ does not scope properly.  It somehow loses the outter class (MPI
  // or PMPI) -- remember, g++ does not do namespaces! -- and thinks that 
  // we are conflicting with Errhandler_fn et al. in comm.h
  // @#$%@#$@#%$@#% g++!!
  typedef void PMPI_Errhandler_fn(Comm&, int*, ...);
  typedef int PMPI_Copy_attr_function(const Comm& oldcomm, int comm_keyval,
				      void* extra_state, void* attribute_val_in,
				      void* attribute_val_out, MPI2CPP_BOOL_T& flag);
  typedef int PMPI_Delete_attr_function(Comm& comm, int comm_keyval, 
					void* attribute_val, void* extra_state);
#define ERRHANDLERFN PMPI_Errhandler_fn
#define COPYATTRFN PMPI_Copy_attr_function
#define DELETEATTRFN PMPI_Delete_attr_function

  // construction
  Comm() { }
  // copy
  Comm(const Comm_Null& data) : Comm_Null(data) { }
  // inter-language operability  
  Comm(const MPI_Comm& data) : Comm_Null(data) { }

  //
  // Point-to-Point
  //

  virtual void Send(const void *buf, int count, 
		    const Datatype & datatype, int dest, int tag) const;

  virtual void Recv(void *buf, int count, const Datatype & datatype,
		    int source, int tag, Status & status) const;

  virtual void Recv(void *buf, int count, const Datatype & datatype,
		    int source, int tag) const;
  
  virtual void Bsend(const void *buf, int count,
		     const Datatype & datatype, int dest, int tag) const;
  
  virtual void Ssend(const void *buf, int count, 
		     const Datatype & datatype, int dest, int tag) const;
  
  virtual void Rsend(const void *buf, int count,
		     const Datatype & datatype, int dest, int tag) const;
  
  virtual Request Isend(const void *buf, int count,
			     const Datatype & datatype,
			     int dest, int tag) const;

  virtual Request Ibsend(const void *buf, int count, const
			      Datatype & datatype, int dest, int tag) const;
 
  virtual Request Issend(const void *buf, int count,
			      const Datatype & datatype, int dest, int tag) const;
  
  virtual Request Irsend(const void *buf, int count,
			      const Datatype & datatype, int dest, int tag) const;
  
  virtual Request Irecv(void *buf, int count,
			     const Datatype & datatype, int source, int tag) const;

  virtual MPI2CPP_BOOL_T Iprobe(int source, int tag, Status & status) const;
  
  virtual MPI2CPP_BOOL_T Iprobe(int source, int tag) const;

  virtual void Probe(int source, int tag, Status & status) const;

  virtual void Probe(int source, int tag) const;

  virtual Prequest Send_init(const void *buf, int count,
				  const  Datatype & datatype,
				  int dest, int tag) const;

  virtual Prequest Bsend_init(const void *buf, int count,
				   const Datatype & datatype,
				   int dest, int tag) const;
  
  virtual Prequest Ssend_init(const void *buf, int count,
				   const Datatype & datatype,
				   int dest, int tag) const;
  
  virtual Prequest Rsend_init(const void *buf, int count,
				   const Datatype & datatype,
				   int dest, int tag) const;

  virtual Prequest Recv_init(void *buf, int count,
				  const Datatype & datatype,
				  int source, int tag) const;
  
  virtual void Sendrecv(const void *sendbuf, int sendcount,
			const Datatype & sendtype, int dest, int sendtag, 
			void *recvbuf, int recvcount, 
			const Datatype & recvtype, int source,
			int recvtag, Status & status) const;
 
  virtual void Sendrecv(const void *sendbuf, int sendcount,
			const Datatype & sendtype, int dest, int sendtag, 
			void *recvbuf, int recvcount, 
			const Datatype & recvtype, int source,
			int recvtag) const;

  virtual void Sendrecv_replace(void *buf, int count,
				const Datatype & datatype, int dest, 
				int sendtag, int source,
				int recvtag, Status & status) const;

  virtual void Sendrecv_replace(void *buf, int count,
				const Datatype & datatype, int dest, 
				int sendtag, int source,
				int recvtag) const;

  //
  // Groups, Contexts, and Communicators
  //
  
  virtual Group Get_group() const;
  
  virtual int Get_size() const;
  
  virtual int Get_rank() const;

  static int Compare(const Comm & comm1, const Comm & comm2);

  virtual void Free(void);
  
  virtual MPI2CPP_BOOL_T Is_inter() const;
  
  //
  //Process Topologies
  //

  virtual int Get_topology() const;
  
  //
  // Environmental Inquiry
  //

  virtual void Abort(int errorcode);

  virtual void Set_errhandler(const Errhandler& errhandler);

  virtual Errhandler Get_errhandler() const;

  static Errhandler Create_errhandler(PMPI_Errhandler_fn* function);

  //
  // Keys and Attributes
  //

  static int Create_keyval(PMPI_Copy_attr_function*,
			   PMPI_Delete_attr_function*,
			   void*);
  
  static void Free_keyval(int& comm_keyval);
  
  virtual void Set_attr(int comm_keyval, const void* attribute_val) const;
  
  virtual MPI2CPP_BOOL_T Get_attr(int comm_keyval, void* attribute_val) const;
  
  virtual void Delete_attr(int comm_keyval);
  
  static int NULL_COPY_FN(const PMPI::Comm& oldcomm, int comm_keyval,
			  void* extra_state, void* attribute_val_in,
			  void* attribute_val_out, MPI2CPP_BOOL_T& flag);
  
  static int DUP_FN(const Comm& oldcomm, int comm_keyval,
		    void* extra_state, void* attribute_val_in,
		    void* attribute_val_out, MPI2CPP_BOOL_T& flag);
  
  static int NULL_DELETE_FN(Comm& comm, int comm_keyval, void* attribute_val,
			    void* extra_state);
private:
  static Status ignored_status;
public: // JGS friends cause portability problems
  static MPI_SGI_Map mpi_comm_map;
  static MPI_SGI_Map key_fn_map;
  Errhandler* my_errhandler;

};

/* process-header: end of pcomm.h */
/* process-header: including perrhandler.h */

class Errhandler {
#if 0
  friend void Real_init();  //see function init below
#endif
public:
  // construction
  inline Errhandler()
    : mpi_errhandler(MPI_ERRHANDLER_NULL) {}
  // inter-language operability
  inline Errhandler(const MPI_Errhandler &i)
    : mpi_errhandler(i) {}
  // copy
  inline Errhandler(const Errhandler& e);

  inline virtual ~Errhandler() {}

  inline Errhandler& operator=(const Errhandler& e);

  // comparison
  inline MPI2CPP_BOOL_T operator==(const Errhandler &a);

  inline MPI2CPP_BOOL_T operator!=(const Errhandler &a) {
    return (MPI2CPP_BOOL_T)!(*this == a); }

  // inter-language operability
  inline Errhandler& operator= (const MPI_Errhandler &i) {
    mpi_errhandler = i; return *this; }
 
  inline operator MPI_Errhandler() const { return mpi_errhandler; }
 
  inline operator MPI_Errhandler*() { return &mpi_errhandler; }
  
  //
  // Errhandler access functions
  //
  
  inline virtual void Free(void);

  Comm::PMPI_Errhandler_fn* handler_fn;

protected:
  MPI_Errhandler mpi_errhandler;

public:
  //this is for ERRORS_THROW_EXCEPTIONS
  //this is called from MPI::Real_init
  // g++ doesn't understand friends so this must be public :(
  inline void init() const {
    (void)MPI_Errhandler_create(&throw_excptn_fctn,
				(MPI_Errhandler *)&mpi_errhandler); 
  }
};
/* process-header: end of perrhandler.h */
/* process-header: including pintracomm.h */

class Intracomm : public Comm {

#if 0 // friends cause portability problems
#if _MPIPP_USENAMESPACE_
  friend void ::op_intercept(void *invec, void *outvec, int *len, MPI_Datatype *datatype);
#endif

#if ! _MPIPP_USENAMESPACE_
  friend void op_intercept(void *invec, void *outvec, int *len, MPI_Datatype *datatype);
#endif
#endif

public:
  // construction
  Intracomm() { }
  // copy
  Intracomm(const Intracomm& data) : Comm(data) { }
  
  // inter-language operability
  inline Intracomm(const MPI_Comm& data);

  //
  // Collective Communication
  //

  virtual void Barrier() const;

  virtual void Bcast(void *buffer, int count, 
		     const Datatype& datatype, int root) const;

  virtual void Gather(const void *sendbuf, int sendcount, 
			    const Datatype & sendtype, 
			    void *recvbuf, int recvcount, 
			    const Datatype & recvtype, int root) const;
  
  virtual void Gatherv(const void *sendbuf, int sendcount, 
			      const Datatype & sendtype, void *recvbuf, 
			      const int recvcounts[], const int displs[], 
			      const Datatype & recvtype, int root) const;

  virtual void Scatter(const void *sendbuf, int sendcount, 
			      const Datatype & sendtype, 
			      void *recvbuf, int recvcount, 
			      const Datatype & recvtype, int root) const;

  virtual void Scatterv(const void *sendbuf, const int sendcounts[], 
			       const int displs[], const Datatype & sendtype,
			       void *recvbuf, int recvcount, 
			       const Datatype & recvtype, int root) const;

  virtual void Allgather(const void *sendbuf, int sendcount, 
			 const Datatype & sendtype, void *recvbuf, 
			 int recvcount, const Datatype & recvtype) const;

  virtual void Allgatherv(const void *sendbuf, int sendcount, 
			  const Datatype & sendtype, void *recvbuf, 
			  const int recvcounts[], const int displs[],
			  const Datatype & recvtype) const;

  virtual void Alltoall(const void *sendbuf, int sendcount, 
			const Datatype & sendtype, void *recvbuf, 
			int recvcount, const Datatype & recvtype) const;

  virtual void Alltoallv(const void *sendbuf, const int sendcounts[], 
			 const int sdispls[], const Datatype & sendtype, 
			 void *recvbuf, const int recvcounts[], 
			 const int rdispls[], const Datatype & recvtype) const;

  virtual void Reduce(const void *sendbuf, void *recvbuf, int count, 
		      const Datatype & datatype, const Op & op, 
		      int root) const;

  virtual void Allreduce(const void *sendbuf, void *recvbuf, int count,
				const Datatype & datatype, const Op & op) const;

  virtual void Reduce_scatter(const void *sendbuf, void *recvbuf, 
				     int recvcounts[], 
				     const Datatype & datatype, 
				     const Op & op) const;

  virtual void Scan(const void *sendbuf, void *recvbuf, int count, 
		    const Datatype & datatype, const Op & op) const;

  Intracomm Dup() const;

#if VIRTUAL_FUNC_RET
  Intracomm&
#else
  Comm&
#endif
  Clone() const;

  virtual Intracomm Create(const Group& group) const;
  
  virtual Intracomm Split(int color, int key) const;

  virtual Intercomm Create_intercomm(int local_leader, const Comm& peer_comm,
					  int remote_leader, int tag) const;
  
  virtual Cartcomm Create_cart(int ndims, const int dims[],
				    const MPI2CPP_BOOL_T periods[], MPI2CPP_BOOL_T reorder) const;
  
  virtual Graphcomm Create_graph(int nnodes, const int index[],
				      const int edges[], MPI2CPP_BOOL_T reorder) const;


protected:

public: // JGS, friends issue
  static Op* current_op;

};
/* process-header: end of pintracomm.h */
/* process-header: including ptopology.h */


class Cartcomm : public Intracomm {
public:
   // construction
  Cartcomm() : Intracomm(MPI_COMM_NULL) { }
  // copy
  Cartcomm(const Cartcomm& data) : Intracomm(data) { }
  // inter-language operability
  inline Cartcomm(const MPI_Comm& data);

  //
  // Groups, Contexts, and Communicators
  //

  Cartcomm Dup() const;

#if VIRTUAL_FUNC_RET
  Cartcomm&
#else
  Comm&
#endif
  Clone() const;

  //
  //  Process Topologies
  //

  virtual int Get_dim() const;

  // JGS KCC gives a warning here because of Comm::Get_topo()
  virtual void Get_topo(int maxdims, int dims[], MPI2CPP_BOOL_T periods[],
			      int coords[]) const;

  virtual int Get_cart_rank(const int coords[]) const;
  
  virtual void Get_coords(int rank, int maxdims, int coords[]) const;

  virtual void Shift(int direction, int disp,
		     int &rank_source, int &rank_dest) const;
  
  virtual Cartcomm Sub(const MPI2CPP_BOOL_T remain_dims[]);

  virtual int Map(int ndims, const int dims[], const MPI2CPP_BOOL_T periods[]) const;

};

//===================================================================
//                    Class Graphcomm
//===================================================================

class Graphcomm : public Intracomm {
public:
  // construction
  Graphcomm() : Intracomm(MPI_COMM_NULL) { }
  // copy
  Graphcomm(const Graphcomm& data) : Intracomm(data) { }
  // inter-language operability
  inline Graphcomm(const MPI_Comm& data);

  //
  // Groups, Contexts, and Communicators
  //

  Graphcomm Dup() const;

#if VIRTUAL_FUNC_RET
  Graphcomm&
#else
  Comm&
#endif
  Clone() const;

  //
  //  Process Topologies
  //

  virtual void Get_dims(int nnodes[], int nedges[]) const;

  virtual void Get_topo(int maxindex, int maxedges, int index[], 
			int edges[]) const;

  virtual int Get_neighbors_count(int rank) const;

  virtual void Get_neighbors(int rank, int maxneighbors, 
			     int neighbors[]) const;

  virtual int Map(int nnodes, const int index[], 
		  const int edges[]) const;

};
/* process-header: end of ptopology.h */
/* process-header: including pintercomm.h */


class Intercomm : public Comm {
public:

  // construction
  Intercomm() : Comm(MPI_COMM_NULL) { }
  // copy
  Intercomm(const Intercomm& data) : Comm(data) { }
  // inter-language operability
  Intercomm(const MPI_Comm& data) : Comm(data) { }

  //
  // Groups, Contexts, and Communicators
  //

  Intercomm Dup() const;

#if VIRTUAL_FUNC_RET
  Intercomm&
#else
  Comm&
#endif
  Clone() const;

  virtual int Get_remote_size() const;

  Group Get_remote_group() const;
  
  virtual Intracomm Merge(MPI2CPP_BOOL_T high);

};
/* process-header: end of pintercomm.h */
  
#if ! _MPIPP_USENAMESPACE_
private:
  PMPI() { }
#endif
};


#endif
/* process-header: end of pmpi++.h */
#endif

#if _MPIPP_USENAMESPACE_
namespace MPI {
#else
class MPI {
public:
#endif

#if ! _MPIPP_USEEXCEPTIONS_
  _MPIPP_EXTERN_ _MPIPP_STATIC_ int errno;
#endif

  class Comm_Null;
  class Comm;
  class Intracomm;
  class Intercomm;
  class Graphcomm;
  class Cartcomm;
  class Datatype;
  class Errhandler;
  class Group;
  class Op;
  class Request;
  class Status;

  typedef MPI_Aint Aint;

/* process-header: including constants.h */


// return  codes
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int SUCCESS;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_BUFFER;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_COUNT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_TYPE;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_TAG ;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_COMM;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_RANK;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_REQUEST;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_ROOT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_GROUP;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_OP;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_TOPOLOGY;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_DIMS;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_ARG;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_UNKNOWN;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_TRUNCATE;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_OTHER;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_INTERN;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_PENDING;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_IN_STATUS;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ERR_LASTCODE;

// assorted constants
_MPIPP_EXTERN_ _MPIPP_STATIC_ const void* BOTTOM;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int PROC_NULL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ANY_SOURCE;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int ANY_TAG;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int UNDEFINED;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int BSEND_OVERHEAD;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int KEYVAL_INVALID;

// error-handling specifiers
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Errhandler  ERRORS_ARE_FATAL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Errhandler  ERRORS_RETURN;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Errhandler  ERRORS_THROW_EXCEPTIONS;

// maximum sizes for strings
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int MAX_PROCESSOR_NAME;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int MAX_ERROR_STRING;

// elementary datatypes (C / C++)
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype CHAR;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype SHORT;          
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype INT;            
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LONG;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype SIGNED_CHAR;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype UNSIGNED_CHAR;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype UNSIGNED_SHORT; 
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype UNSIGNED;       
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype UNSIGNED_LONG;  
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype FLOAT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype DOUBLE;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LONG_DOUBLE;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype BYTE;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype PACKED;

// datatypes for reductions functions (C / C++)
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype FLOAT_INT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype DOUBLE_INT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LONG_INT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype TWOINT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype SHORT_INT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LONG_DOUBLE_INT;

#if MPI2CPP_FORTRAN
// elementary datatype (Fortran)
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype INTEGER;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype REAL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype DOUBLE_PRECISION;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype F_COMPLEX;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LOGICAL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype CHARACTER;

// datatype for reduction functions (Fortran)
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype TWOREAL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype TWODOUBLE_PRECISION;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype TWOINTEGER;
#endif

#if MPI2CPP_ALL_OPTIONAL_FORTRAN
// optional datatypes (Fortran)
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype INTEGER1;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype INTEGER2;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype INTEGER4;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype REAL2;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype REAL4;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype REAL8;
#elif MPI2CPP_SOME_OPTIONAL_FORTRAN
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype INTEGER2;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype REAL2;
#endif

#if MPI2CPP_OPTIONAL_C
// optional datatype (C / C++)
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LONG_LONG;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype UNSIGNED_LONG_LONG;
#endif

#if 0
// c++ types
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype BOOL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype COMPLEX;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype DOUBLE_COMPLEX;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LONG_DOUBLE_COMPLEX;
#endif

// special datatypes for contstruction of derived datatypes
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype UB;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype LB;

// reserved communicators
// JGS these can not be const because Set_errhandler is not const
_MPIPP_EXTERN_ _MPIPP_STATIC_ Intracomm COMM_WORLD;
_MPIPP_EXTERN_ _MPIPP_STATIC_ Intracomm COMM_SELF;

// results of communicator and group comparisons
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int IDENT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int CONGRUENT;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int SIMILAR;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int UNEQUAL;

// environmental inquiry keys
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int TAG_UB;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int IO;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int HOST;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int WTIME_IS_GLOBAL;

// collective operations
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op MAX;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op MIN;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op SUM;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op PROD;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op MAXLOC;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op MINLOC;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op BAND;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op BOR;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op BXOR;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op LAND;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op LOR;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op LXOR;

// null handles
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Group        GROUP_NULL;
//_MPIPP_EXTERN_ _MPIPP_STATIC_ const Comm         COMM_NULL;
//_MPIPP_EXTERN_ _MPIPP_STATIC_ const MPI_Comm     COMM_NULL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ Comm_Null          COMM_NULL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Datatype     DATATYPE_NULL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ Request            REQUEST_NULL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Op           OP_NULL;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Errhandler   ERRHANDLER_NULL;  

// empty group
_MPIPP_EXTERN_ _MPIPP_STATIC_ const Group  GROUP_EMPTY;

// topologies
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int GRAPH;
_MPIPP_EXTERN_ _MPIPP_STATIC_ const int CART;


/* process-header: end of constants.h */
/* process-header: including functions.h */

//
// Point-to-Point Communication
//

_MPIPP_STATIC_ void 
Attach_buffer(void* buffer, int size);

_MPIPP_STATIC_ int 
Detach_buffer(void*& buffer);

//
// Process Topologies
//

_MPIPP_STATIC_ void
Compute_dims(int nnodes, int ndims, int dims[]);

//
// Environmental Inquiry
//

_MPIPP_STATIC_ void 
Get_processor_name(char*& name, int& resultlen);

_MPIPP_STATIC_ void
Get_error_string(int errorcode, char*& string, int& resultlen);

_MPIPP_STATIC_ int 
Get_error_class(int errorcode);

_MPIPP_STATIC_ double 
Wtime();

_MPIPP_STATIC_ double 
Wtick();

_MPIPP_STATIC_ void
Init(int& argc, char**& argv);

_MPIPP_STATIC_ void
Init();

_MPIPP_STATIC_ void
Real_init();

_MPIPP_STATIC_ void
Finalize();

_MPIPP_STATIC_ MPI2CPP_BOOL_T
Is_initialized();

//
// Profiling
//

_MPIPP_STATIC_ void
Pcontrol(const int level, ...);


// JGS defer to MPI-2
//inline _MPIPP_STATIC_ void
//Get_version(int& version, int& subversion);

_MPIPP_STATIC_ _REAL_MPI_::Aint
Get_address(void* location);
/* process-header: end of functions.h */
/* process-header: including datatype.h */


class Datatype {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Datatype;
#endif
public:

#if _MPIPP_PROFILING_

  // construction
  inline Datatype() { }

  // inter-language operability
  inline Datatype(const MPI_Datatype &i) : pmpi_datatype(i) { }

  // copy / assignment
  inline Datatype(const Datatype& dt) : pmpi_datatype(dt.pmpi_datatype) { }

  inline Datatype(const PMPI::Datatype& dt) : pmpi_datatype(dt) { }
  
  inline virtual ~Datatype() {}

  inline Datatype& operator=(const Datatype& dt) {
    pmpi_datatype = dt.pmpi_datatype; return *this; }

  // comparison
  inline MPI2CPP_BOOL_T operator== (const Datatype &a) const
    { return (MPI2CPP_BOOL_T) (pmpi_datatype == a); }

  inline MPI2CPP_BOOL_T operator!= (const Datatype &a) const
    { return (MPI2CPP_BOOL_T) !(*this == a); }

  // inter-language operability
  inline Datatype& operator= (const MPI_Datatype &i) 
    { pmpi_datatype = i; return *this; }

  inline operator MPI_Datatype() const { return (MPI_Datatype)pmpi_datatype; }
  //  inline operator MPI_Datatype* ()/* JGS const */ { return pmpi_datatype; }

  inline operator const PMPI::Datatype&() const { return pmpi_datatype; }

  inline const PMPI::Datatype& pmpi() const { return pmpi_datatype; }

#else

  // construction / destruction
  inline Datatype() : mpi_datatype(MPI_DATATYPE_NULL) { }
  inline virtual ~Datatype() {}
  // inter-language operability
  inline Datatype(const MPI_Datatype &i) : mpi_datatype(i) { }

  // copy / assignment
  inline Datatype(const Datatype& dt) : mpi_datatype(dt.mpi_datatype) { }
  inline Datatype& operator=(const Datatype& dt) {
    mpi_datatype = dt.mpi_datatype; return *this; }

  // comparison
  inline MPI2CPP_BOOL_T operator== (const Datatype &a) const
    { return (MPI2CPP_BOOL_T) (mpi_datatype == a.mpi_datatype); }

  inline MPI2CPP_BOOL_T operator!= (const Datatype &a) const
    { return (MPI2CPP_BOOL_T) !(*this == a); }

  // inter-language operability
  inline Datatype& operator= (const MPI_Datatype &i) 
    { mpi_datatype = i; return *this; }

  inline operator MPI_Datatype () const { return mpi_datatype; }
  // inline operator MPI_Datatype* ()/* JGS const */ { return &mpi_datatype; }

#endif
  
  //
  // Point-to-Point Communication
  //
  
  virtual Datatype Create_contiguous(int count) const;
  
  virtual Datatype Create_vector(int count, int blocklength,
				 int stride) const;
  
  virtual Datatype Create_indexed(int count,
				  const int array_of_blocklengths[], 
				  const int array_of_displacements[]) const;

  static Datatype Create_struct(int count, const int array_of_blocklengths[],
				const Aint array_of_displacements[],
				const Datatype array_if_types[]);
  
  virtual Datatype Create_hindexed(int count, const int array_of_blocklengths[],
				   const Aint array_of_displacements[]) const;

  virtual Datatype Create_hvector(int count, int blocklength, Aint stride) const;

  virtual int Get_size() const;
  
  virtual void Get_extent(Aint& lb, Aint& extent) const;

  virtual void Commit();
  
  virtual void Free();

  virtual void Pack(const void* inbuf, int incount, void *outbuf, 
		    int outsize, int& position, const Comm &comm) const;

  virtual void Unpack(const void* inbuf, int insize, void *outbuf, int outcount,
		      int& position, const Comm& comm) const;

  virtual int Pack_size(int incount, const Comm& comm) const;

#if _MPIPP_PROFILING_
private:
  PMPI::Datatype pmpi_datatype;
#else
protected:
  MPI_Datatype mpi_datatype;
#endif


};

/* process-header: end of datatype.h */

  typedef void User_function(const void* invec, void* inoutvec, int len,
			     const Datatype& datatype);

/* process-header: including exception.h */


class Exception {
public:

#if _MPIPP_PROFILING_

  inline Exception(int ec) : pmpi_exception(ec) { }

  int Get_error_code() const;
  
  int Get_error_class() const;
  
  const char* Get_error_string() const;
 
#else

  inline Exception(int ec) : error_code(ec) {
    (void)MPI_Error_class(error_code, &error_class);
    int resultlen;
    error_string = new char[MAX_ERROR_STRING];
    (void)MPI_Error_string(error_code, error_string, &resultlen);
  }

  inline int Get_error_code() const { return error_code; }

  inline int Get_error_class() const { return error_class; }
  
  inline const char* Get_error_string() const { return error_string; }

#endif
 
protected:
#if _MPIPP_PROFILING_
  PMPI::Exception pmpi_exception;
#else
  int error_code;
  char* error_string;
  int error_class;
#endif
};
/* process-header: end of exception.h */
/* process-header: including op.h */


class Op {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Op;
#endif
public:

  // construction
  Op();
  Op(const MPI_Op &i);
  Op(const Op& op);
#if _MPIPP_PROFILING_
  Op(const PMPI::Op& op) : pmpi_op(op) { }
#endif
  // destruction
  virtual ~Op();
  // assignment
  Op& operator=(const Op& op);
  Op& operator= (const MPI_Op &i);
  // comparison
  inline MPI2CPP_BOOL_T operator== (const Op &a);
  inline MPI2CPP_BOOL_T operator!= (const Op &a);
  // conversion functions for inter-language operability
  inline operator MPI_Op () const;
  //  inline operator MPI_Op* (); //JGS const
#if _MPIPP_PROFILING_
  inline operator const PMPI::Op&() const { return pmpi_op; }
#endif
  // Collective Communication
  //JGS took const out
  virtual void Init(User_function *func, MPI2CPP_BOOL_T commute);
  virtual void Free();
 
#if ! _MPIPP_PROFILING_
  User_function *op_user_function; //JGS move to private
protected:
  MPI_Op mpi_op;
#endif

#if _MPIPP_PROFILING_
private:
  PMPI::Op pmpi_op;
#endif

};

/* process-header: end of op.h */
/* process-header: including status.h */


class Status {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Status;
#endif
  friend class MPI::Comm; //so I can access pmpi_status data member in comm.cc
  friend class MPI::Request; //and also from request.cc

public:
#if _MPIPP_PROFILING_

  // construction / destruction
  Status() { }
  virtual ~Status() {}

  // copy / assignment
  Status(const Status& data) : pmpi_status(data.pmpi_status) { }

  Status(const MPI_Status &i) : pmpi_status(i) { }

  Status& operator=(const Status& data) {
    pmpi_status = data.pmpi_status; return *this; }

  // comparison, don't need for status

  // inter-language operability
  Status& operator= (const MPI_Status &i) {
    pmpi_status = i; return *this; }
  operator MPI_Status () const { return pmpi_status; }
  //  operator MPI_Status* () const { return pmpi_status; }
  operator const PMPI::Status&() const { return pmpi_status; }

#else

  Status() { }
  // copy
  Status(const Status& data) : mpi_status(data.mpi_status) { }

  Status(const MPI_Status &i) : mpi_status(i) { }

  virtual ~Status() {}

  Status& operator=(const Status& data) {
    mpi_status = data.mpi_status; return *this; }

  // comparison, don't need for status

  // inter-language operability
  Status& operator= (const MPI_Status &i) {
    mpi_status = i; return *this; }
  operator MPI_Status () const { return mpi_status; }
  //  operator MPI_Status* () const { return (MPI_Status*)&mpi_status; }

#endif

  //
  // Point-to-Point Communication
  //

  virtual int Get_count(const Datatype& datatype) const;

  virtual MPI2CPP_BOOL_T Is_cancelled() const;

  virtual int Get_elements(const Datatype& datatype) const;

  //
  // Status Access
  //
  virtual int Get_source() const;

  virtual void Set_source(int source);

  virtual int Get_tag() const;
  
  virtual void Set_tag(int tag);
  
  virtual int Get_error() const;

  virtual void Set_error(int error);

protected:
#if _MPIPP_PROFILING_
  PMPI::Status pmpi_status;
#else
  MPI_Status mpi_status;
#endif

};
/* process-header: end of status.h */
/* process-header: including request.h */


class Request {
#if _MPIPP_PROFILING_
  //    friend class PMPI::Request;
#endif
public:
#if _MPIPP_PROFILING_

  // construction
  Request() { }
  Request(const MPI_Request &i) : pmpi_request(i) { }

  // copy / assignment
  Request(const Request& r) : pmpi_request(r.pmpi_request) { }

  Request(const PMPI::Request& r) : pmpi_request(r) { }

  virtual ~Request() {}

  Request& operator=(const Request& r) {
    pmpi_request = r.pmpi_request; return *this; }

  // comparison
  MPI2CPP_BOOL_T operator== (const Request &a) 
  { return (MPI2CPP_BOOL_T)(pmpi_request == a); }
  MPI2CPP_BOOL_T operator!= (const Request &a) 
  { return (MPI2CPP_BOOL_T)!(*this == a); }

  // inter-language operability
  Request& operator= (const MPI_Request &i) {
    pmpi_request = i; return *this;  }

  operator MPI_Request () const { return pmpi_request; }
  //  operator MPI_Request* () const { return pmpi_request; }
  operator const PMPI::Request&() const { return pmpi_request; }

#else

  // construction / destruction
  Request() { mpi_request = MPI_REQUEST_NULL; }
  virtual ~Request() {}
  Request(const MPI_Request &i) : mpi_request(i) { }

  // copy / assignment
  Request(const Request& r) : mpi_request(r.mpi_request) { }

  Request& operator=(const Request& r) {
    mpi_request = r.mpi_request; return *this; }

  // comparison
  MPI2CPP_BOOL_T operator== (const Request &a) 
  { return (MPI2CPP_BOOL_T)(mpi_request == a.mpi_request); }
  MPI2CPP_BOOL_T operator!= (const Request &a) 
  { return (MPI2CPP_BOOL_T)!(*this == a); }

  // inter-language operability
  Request& operator= (const MPI_Request &i) {
    mpi_request = i; return *this; }
  operator MPI_Request () const { return mpi_request; }
  //  operator MPI_Request* () const { return (MPI_Request*)&mpi_request; }

#endif

  //
  // Point-to-Point Communication
  //

  virtual void Wait(Status &status);

  virtual void Wait();

  virtual MPI2CPP_BOOL_T Test(Status &status);

  virtual MPI2CPP_BOOL_T Test();

  virtual void Free(void);

  static int Waitany(int count, Request array[], Status& status);

  static int Waitany(int count, Request array[]);

  static MPI2CPP_BOOL_T Testany(int count, Request array[], int& index, Status& status);

  static MPI2CPP_BOOL_T Testany(int count, Request array[], int& index);

  static void Waitall(int count, Request req_array[], Status stat_array[]);
 
  static void Waitall(int count, Request req_array[]);

  static MPI2CPP_BOOL_T Testall(int count, Request req_array[], Status stat_array[]);
 
  static MPI2CPP_BOOL_T Testall(int count, Request req_array[]);

  static int Waitsome(int incount, Request req_array[],
			     int array_of_indices[], Status stat_array[]) ;

  static int Waitsome(int incount, Request req_array[],
			     int array_of_indices[]);

  static int Testsome(int incount, Request req_array[],
			     int array_of_indices[], Status stat_array[]);

  static int Testsome(int incount, Request req_array[],
			     int array_of_indices[]);

  virtual void Cancel(void) const;


protected:
#if ! _MPIPP_PROFILING_
  MPI_Request mpi_request;
#endif

private:
  static Status ignored_status;

#if _MPIPP_PROFILING_
  PMPI::Request pmpi_request;
#endif 

};


class Prequest : public Request {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Prequest;
#endif
public:

  Prequest() { }

#if _MPIPP_PROFILING_
  Prequest(const Request& p) : pmpi_request(p), Request(p) { }

  Prequest(const PMPI::Prequest& r) : pmpi_request(r), Request((const PMPI::Request&)r) { }

  Prequest(const MPI_Request &i) : pmpi_request(i), Request(i) { }
  
  virtual ~Prequest() { }

  Prequest& operator=(const Request& r) {
    Request::operator=(r);
    pmpi_request = (PMPI::Prequest)r; return *this; }

  Prequest& operator=(const Prequest& r) {
    Request::operator=(r);
    pmpi_request = r.pmpi_request; return *this; }
#else
  Prequest(const Request& p) : Request(p) { }

  Prequest(const MPI_Request &i) : Request(i) { }

  virtual ~Prequest() { }

  Prequest& operator=(const Request& r) {
    mpi_request = r; return *this; }

  Prequest& operator=(const Prequest& r) {
    mpi_request = r.mpi_request; return *this; }
#endif

  virtual void Start();

  static void Startall(int count, Prequest array_of_requests[]);

#if _MPIPP_PROFILING_
private:
  PMPI::Prequest pmpi_request;
#endif 
};
/* process-header: end of request.h */
/* process-header: including group.h */

class Group {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Group;
#endif
public:

#if _MPIPP_PROFILING_

  // construction
  inline Group() { }
  inline Group(const MPI_Group &i) : pmpi_group(i) { }
  // copy
  inline Group(const Group& g) : pmpi_group(g.pmpi_group) { }

  inline Group(const PMPI::Group& g) : pmpi_group(g) { }

  inline virtual ~Group() {}

  Group& operator=(const Group& g) {
    pmpi_group = g.pmpi_group; return *this;
  }

  // comparison
  inline MPI2CPP_BOOL_T operator== (const Group &a) {
    return (MPI2CPP_BOOL_T)(pmpi_group == a);
  }
  inline MPI2CPP_BOOL_T operator!= (const Group &a) { 
    return (MPI2CPP_BOOL_T)!(*this == a);
  }
 
  // inter-language operability
  Group& operator= (const MPI_Group &i) { pmpi_group = i; return *this; }
  inline operator MPI_Group () const { return pmpi_group.mpi(); }
  //  inline operator MPI_Group* () const { return pmpi_group; }
  inline operator const PMPI::Group&() const { return pmpi_group; }

  const PMPI::Group& pmpi() { return pmpi_group; }
#else

  // construction
  inline Group() : mpi_group(MPI_GROUP_NULL) { }
  inline Group(const MPI_Group &i) : mpi_group(i) { }

  // copy
  inline Group(const Group& g) : mpi_group(g.mpi_group) { }

  inline virtual ~Group() {}

  inline Group& operator=(const Group& g) { mpi_group = g.mpi_group; return *this; }

  // comparison
  inline MPI2CPP_BOOL_T operator== (const Group &a) { return (MPI2CPP_BOOL_T)(mpi_group == a.mpi_group); }
  inline MPI2CPP_BOOL_T operator!= (const Group &a) { return (MPI2CPP_BOOL_T)!(*this == a); }
 
  // inter-language operability
  inline Group& operator= (const MPI_Group &i) { mpi_group = i; return *this; }
  inline operator MPI_Group () const { return mpi_group; }
  //  inline operator MPI_Group* () const { return (MPI_Group*)&mpi_group; }

  inline MPI_Group mpi() const { return mpi_group; }

#endif

  //
  // Groups, Contexts, and Communicators
  //

  virtual int Get_size() const;
  
  virtual int Get_rank() const;
  
  static void Translate_ranks (const Group& group1, int n, const int ranks1[], 
			       const Group& group2, int ranks2[]);
  
  static int Compare(const Group& group1, const Group& group2);
  
  static Group Union(const Group &group1, const Group &group2);
  
  static Group Intersect(const Group &group1, const Group &group2);
  
  static Group Difference(const Group &group1, const Group &group2);
  
  virtual Group Incl(int n, const int ranks[]) const;
  
  virtual Group Excl(int n, const int ranks[]) const;
  
  virtual Group Range_incl(int n, const int ranges[][3]) const;
  
  virtual Group Range_excl(int n, const int ranges[][3]) const;
  
  virtual void Free();

protected:
#if ! _MPIPP_PROFILING_
  MPI_Group mpi_group;
#endif

#if _MPIPP_PROFILING_
private:
  PMPI::Group pmpi_group;
#endif

};

/* process-header: end of group.h */
/* process-header: including comm.h */

#define COMM_NOT_ABSTRACT 1

class Comm_Null {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Comm_Null;
#endif
public:

#if _MPIPP_PROFILING_

  // construction
  inline Comm_Null() { }
  // copy
  inline Comm_Null(const Comm_Null& data) : pmpi_comm(data.pmpi_comm) { }
  // inter-language operability  
  inline Comm_Null(const MPI_Comm& data) : pmpi_comm(data) { }

  inline Comm_Null(const PMPI::Comm_Null& data) : pmpi_comm(data) { }

  // destruction
  //JGS  virtual inline ~Comm_Null() { }

  inline Comm_Null& operator=(const Comm_Null& data) {
    pmpi_comm = data.pmpi_comm; 
    return *this;
  }

  // comparison
  inline MPI2CPP_BOOL_T operator==(const Comm_Null& data) const {
    return (MPI2CPP_BOOL_T) (pmpi_comm == data.pmpi_comm); }

  inline MPI2CPP_BOOL_T operator!=(const Comm_Null& data) const {
    return (MPI2CPP_BOOL_T) (pmpi_comm != data.pmpi_comm);}

  // inter-language operability (conversion operators)
  inline operator MPI_Comm() const { return pmpi_comm; }
  //  inline operator MPI_Comm*() /*const JGS*/ { return pmpi_comm; }
  inline operator const PMPI::Comm_Null&() const { return pmpi_comm; }

#else

  // construction
  inline Comm_Null() : mpi_comm(MPI_COMM_NULL) { }
  // copy
  inline Comm_Null(const Comm_Null& data) : mpi_comm(data.mpi_comm) { }
  // inter-language operability  
  inline Comm_Null(const MPI_Comm& data) : mpi_comm(data) { }

  // destruction
  //JGS virtual inline ~Comm_Null() { }

 // comparison
  // JGS make sure this is right (in other classes too)
  inline MPI2CPP_BOOL_T operator==(const Comm_Null& data) const {
    return (MPI2CPP_BOOL_T) (mpi_comm == data.mpi_comm); }

  inline MPI2CPP_BOOL_T operator!=(const Comm_Null& data) const {
    return (MPI2CPP_BOOL_T) !(*this == data);}

  // inter-language operability (conversion operators)
  inline operator MPI_Comm() const { return mpi_comm; }

#endif

  
protected:

#if _MPIPP_PROFILING_
  PMPI::Comm_Null pmpi_comm;
#else
  MPI_Comm mpi_comm;
#endif
  

};


class Comm : public Comm_Null {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Comm;
#else


#if 0
  // JGS, screw it. Between the friend differences, the
  // function differences, and the :: problem with egcs
  // this is not worth it. I'm just going to make the stuff
  // these functions need public

#if _MPIPP_USENAMESPACE_
  //JGS, when using namespaces, the :: are required. For some strange
  //reason they are not for nested classes.

#if MPI2CPP_IBM_SP
  friend void ::errhandler_intercept(MPI_Comm * mpi_comm, int * err, char*, int*, int*);
#else
  friend void ::errhandler_intercept(MPI_Comm * mpi_comm, int* err, ...);
#endif
  friend int ::copy_attr_intercept(MPI_Comm oldcomm, int keyval, 
				 void *extra_state, void *attribute_val_in, 
				 void *attribute_val_out, int *flag);
  friend int ::delete_attr_intercept(MPI_Comm comm, int keyval, 
				   void *attribute_val, void *extra_state);
#endif

#if ! _MPIPP_USENAMESPACE_

#if MPI2CPP_IBM_SP
  friend void errhandler_intercept(MPI_Comm * mpi_comm, int * err, char*, int*, int*);
#else
  friend void errhandler_intercept(MPI_Comm * mpi_comm, int* err, ...);
#endif
  friend int copy_attr_intercept(MPI_Comm oldcomm, int keyval, 
				 void *extra_state, void *attribute_val_in, 
				 void *attribute_val_out, int *flag);
  friend int delete_attr_intercept(MPI_Comm comm, int keyval, 
				   void *attribute_val, void *extra_state);
#endif

#endif

#endif

public:

  typedef void Errhandler_fn(Comm&, int*, ...);
  typedef int Copy_attr_function(const Comm& oldcomm, int comm_keyval,
				 void* extra_state, void* attribute_val_in,
				 void* attribute_val_out, MPI2CPP_BOOL_T& flag);
  typedef int Delete_attr_function(Comm& comm, int comm_keyval, void* attribute_val,
				   void* extra_state);
#if !_MPIPP_PROFILING_
#define ERRHANDLERFN Errhandler_fn
#define COPYATTRFN Copy_attr_function
#define DELETEATTRFN Delete_attr_function
#endif

  // construction
  Comm() { }

  // copy
  Comm(const Comm_Null& data) : Comm_Null(data) { }

#if _MPIPP_PROFILING_
  Comm(const Comm& data) : Comm_Null(data) { }

  // inter-language operability
  Comm(const MPI_Comm& data) : pmpi_comm(data), Comm_Null(data) { }

  Comm(const PMPI::Comm& data) : pmpi_comm(data), Comm_Null((const PMPI::Comm_Null&)data) { }

  operator const PMPI::Comm&() const { return pmpi_comm; }

  // assignment
  Comm& operator=(const Comm& data) {
    this->Comm_Null::operator=(data);
    pmpi_comm = data.pmpi_comm; 
    return *this;
  }
  Comm& operator=(const Comm_Null& data) {
    this->Comm_Null::operator=(data);
    MPI_Comm tmp = data;
    pmpi_comm = tmp; 
    return *this;
  }
  // inter-language operability
  Comm& operator=(const MPI_Comm& data) {
    this->Comm_Null::operator=(data);
    pmpi_comm = data;
    return *this;
  }

#else
  Comm(const Comm& data) : Comm_Null(data.mpi_comm) { }
  // inter-language operability
  Comm(const MPI_Comm& data) : Comm_Null(data) { }
#endif


  //
  // Point-to-Point
  //

  virtual void Send(const void *buf, int count, 
		    const Datatype & datatype, int dest, int tag) const;

  virtual void Recv(void *buf, int count, const Datatype & datatype,
		    int source, int tag, Status & status) const;


  virtual void Recv(void *buf, int count, const Datatype & datatype,
		    int source, int tag) const;
  
  virtual void Bsend(const void *buf, int count,
		     const Datatype & datatype, int dest, int tag) const;
  
  virtual void Ssend(const void *buf, int count, 
			   const Datatype & datatype, int dest, int tag) const ;

  virtual void Rsend(const void *buf, int count,
		     const Datatype & datatype, int dest, int tag) const;
  
  virtual Request Isend(const void *buf, int count,
			const Datatype & datatype, int dest, int tag) const;
  
  virtual Request Ibsend(const void *buf, int count, const
			 Datatype & datatype, int dest, int tag) const;
  
  virtual Request Issend(const void *buf, int count,
			 const Datatype & datatype, int dest, int tag) const;
  
  virtual Request Irsend(const void *buf, int count,
			 const Datatype & datatype, int dest, int tag) const;

  virtual Request Irecv(void *buf, int count,
			const Datatype & datatype, int source, int tag) const;

  virtual MPI2CPP_BOOL_T Iprobe(int source, int tag, Status & status) const;

  virtual MPI2CPP_BOOL_T Iprobe(int source, int tag) const;

  virtual void Probe(int source, int tag, Status & status) const;
  
  virtual void Probe(int source, int tag) const;
  
  virtual Prequest Send_init(const void *buf, int count,
			     const Datatype & datatype, int dest, int tag) const;
  
  virtual Prequest Bsend_init(const void *buf, int count,
			      const Datatype & datatype, int dest, int tag) const;
  
  virtual Prequest Ssend_init(const void *buf, int count,
			      const Datatype & datatype, int dest, int tag) const;
  
  virtual Prequest Rsend_init(const void *buf, int count,
			      const Datatype & datatype, int dest, int tag) const;
  
  virtual Prequest Recv_init(void *buf, int count,
			     const Datatype & datatype, int source, int tag) const;
  
  virtual void Sendrecv(const void *sendbuf, int sendcount,
			const Datatype & sendtype, int dest, int sendtag, 
			void *recvbuf, int recvcount, 
			const Datatype & recvtype, int source,
			int recvtag, Status & status) const;
  
  virtual void Sendrecv(const void *sendbuf, int sendcount,
			const Datatype & sendtype, int dest, int sendtag, 
			void *recvbuf, int recvcount, 
			const Datatype & recvtype, int source,
			int recvtag) const;

  virtual void Sendrecv_replace(void *buf, int count,
				const Datatype & datatype, int dest, 
				int sendtag, int source,
				int recvtag, Status & status) const;

  virtual void Sendrecv_replace(void *buf, int count,
				const Datatype & datatype, int dest, 
				int sendtag, int source,
				int recvtag) const;
  
  //
  // Groups, Contexts, and Communicators
  //

  virtual Group Get_group() const;
  
  virtual int Get_size() const;

  virtual int Get_rank() const;
  
  static int Compare(const Comm & comm1, const Comm & comm2);
  
  virtual Comm& Clone() const = 0;

  virtual void Free(void);
  
  virtual MPI2CPP_BOOL_T Is_inter() const;
  
  //
  //Process Topologies
  //
  
  virtual int Get_topology() const;
  
  //
  // Environmental Inquiry
  //
  
  virtual void Abort(int errorcode);

  //
  // Errhandler
  //

  virtual void Set_errhandler(const Errhandler& errhandler);

  virtual Errhandler Get_errhandler() const;

  //JGS took out const below from fn arg
  static Errhandler Create_errhandler(Comm::Errhandler_fn* function);

  //
  // Keys and Attributes
  //

//JGS I took the const out because it causes problems when trying to
//call this function with the predefined NULL_COPY_FN etc.
  static int Create_keyval(Copy_attr_function* comm_copy_attr_fn,
			   Delete_attr_function* comm_delete_attr_fn,
			   void* extra_state);
  
  static void Free_keyval(int& comm_keyval);

  virtual void Set_attr(int comm_keyval, const void* attribute_val) const;

  virtual MPI2CPP_BOOL_T Get_attr(int comm_keyval, void* attribute_val) const;
  
  virtual void Delete_attr(int comm_keyval);

  static int NULL_COPY_FN(const Comm& oldcomm, int comm_keyval,
			  void* extra_state, void* attribute_val_in,
			  void* attribute_val_out, MPI2CPP_BOOL_T& flag);
  
  static int DUP_FN(const Comm& oldcomm, int comm_keyval,
		    void* extra_state, void* attribute_val_in,
		    void* attribute_val_out, MPI2CPP_BOOL_T& flag);
  
  static int NULL_DELETE_FN(Comm& comm, int comm_keyval, void* attribute_val,
			    void* extra_state);


  //#if _MPIPP_PROFILING_
  //  virtual const PMPI::Comm& get_pmpi_comm() const { return pmpi_comm; }
  //#endif

private:
#if _MPIPP_PROFILING_
  PMPI::Comm pmpi_comm;
#endif

  static Status ignored_status;

#if ! _MPIPP_PROFILING_
public: // JGS hmmm, these used by errhandler_intercept
        // should make it a friend
  static MPI_SGI_Map mpi_comm_map;
  Errhandler* my_errhandler;
  static MPI_SGI_Map key_fn_map;

  void init() {
    my_errhandler = (Errhandler*)0;
  }
#endif

};



/* process-header: end of comm.h */
/* process-header: including errhandler.h */

class Errhandler {
#if 0 // JGS compilers hate friends :(
#if _MPIPP_USENAMESPACE_
  friend void _REAL_MPI_::Real_init();  //see function init below
#else
  friend class _REAL_MPI_; //g++ won't except above friend function
#endif
#endif

public:

#if _MPIPP_PROFILING_

  // construction / destruction
  inline Errhandler() { }

  inline virtual ~Errhandler() {}

  inline Errhandler(const MPI_Errhandler &i)
    : pmpi_errhandler(i) { }

 // copy / assignment
  inline Errhandler(const Errhandler& e)
    : pmpi_errhandler(e.pmpi_errhandler) { }

  inline Errhandler(const PMPI::Errhandler& e)
    : pmpi_errhandler(e) { }

  inline Errhandler& operator=(const Errhandler& e) {
    pmpi_errhandler = e.pmpi_errhandler; return *this; }

  // comparison
  inline MPI2CPP_BOOL_T operator==(const Errhandler &a) {
    return (MPI2CPP_BOOL_T)(pmpi_errhandler == a); }
  
  inline MPI2CPP_BOOL_T operator!=(const Errhandler &a) {
    return (MPI2CPP_BOOL_T)!(*this == a); }

  // inter-language operability
  inline Errhandler& operator= (const MPI_Errhandler &i) {
    pmpi_errhandler = i; return *this; }
 
  inline operator MPI_Errhandler() const { return pmpi_errhandler; }
 
  //  inline operator MPI_Errhandler*() { return pmpi_errhandler; }
  
  inline operator const PMPI::Errhandler&() const { return pmpi_errhandler; }

#else

  // construction / destruction
  inline Errhandler()
    : mpi_errhandler(MPI_ERRHANDLER_NULL) {}

  inline virtual ~Errhandler() {}

  inline Errhandler(const MPI_Errhandler &i)
    : mpi_errhandler(i) {}

 // copy / assignment
  inline Errhandler(const Errhandler& e)
    : handler_fn(e.handler_fn), mpi_errhandler(e.mpi_errhandler) { }

  inline Errhandler& operator=(const Errhandler& e)
  {
    mpi_errhandler = e.mpi_errhandler;
    handler_fn = e.handler_fn;
    return *this;
  }

  // comparison
  inline MPI2CPP_BOOL_T operator==(const Errhandler &a) {
    return (MPI2CPP_BOOL_T)(mpi_errhandler == a.mpi_errhandler); }
  
  inline MPI2CPP_BOOL_T operator!=(const Errhandler &a) {
    return (MPI2CPP_BOOL_T)!(*this == a); }

  // inter-language operability
  inline Errhandler& operator= (const MPI_Errhandler &i) {
    mpi_errhandler = i; return *this; }
 
  inline operator MPI_Errhandler() const { return mpi_errhandler; }
 
  //  inline operator MPI_Errhandler*() { return &mpi_errhandler; }
  
#endif

  //
  // Errhandler access functions
  //
  
  virtual void Free();

#if !_MPIPP_PROFILING_
  Comm::Errhandler_fn* handler_fn;
#endif

protected:
#if _MPIPP_PROFILING_
  PMPI::Errhandler pmpi_errhandler;
#else
  MPI_Errhandler mpi_errhandler;
#endif


public:
  // took out the friend decls
  //private:

  //this is for ERRORS_THROW_EXCEPTIONS
  //this is called from MPI::Real_init
  inline void init() const {
#if ! _MPIPP_PROFILING_
    (void)MPI_Errhandler_create(&throw_excptn_fctn,
				(MPI_Errhandler *)&mpi_errhandler); 
#else
    pmpi_errhandler.init();
#endif
  }

};


/* process-header: end of errhandler.h */
/* process-header: including intracomm.h */

class Intracomm : public Comm {

#if 0
  // JGS to many portability issues with these friend decls (see comm.h)
  // going to just make some stuff public
#if ! _MPIPP_PROFILING_
#if _MPIPP_USENAMESPACE_
  friend void ::op_intercept(void *invec, void *outvec, int *len, MPI_Datatype *datatype);
#endif

#if ! _MPIPP_USENAMESPACE_
  friend void op_intercept(void *invec, void *outvec, int *len, MPI_Datatype *datatype);
#endif

#endif // #if ! _MPIPP_PROFILING_

#endif // #if 0

public:

  // construction
  Intracomm() { }
  // copy
  Intracomm(const Comm_Null& data) : Comm(data) { }
  // inter-language operability

#if _MPIPP_PROFILING_
  Intracomm(const Intracomm& data) : Comm(data) { }

  //NOTE: it is extremely important that Comm(data) happens below
  //  because there is a not only pmpi_comm in this Intracomm but
  //  there is also a pmpi_comm in the inherited Comm part. Both
  //  of these pmpi_comm's need to be initialized with the same
  //  MPI_Comm object. Also the assignment operators must take this
  //  into account.
  Intracomm(const MPI_Comm& data) : pmpi_comm(data), Comm(data) { }
  
  Intracomm(const PMPI::Intracomm& data) : pmpi_comm(data), Comm((const PMPI::Comm&)data) { }

  // assignment
  Intracomm& operator=(const Intracomm& data) {
    Comm::operator=(data);
    pmpi_comm = data.pmpi_comm; 
    return *this;
  }
  Intracomm& operator=(const Comm_Null& data) {
    Comm::operator=(data);
    pmpi_comm = (PMPI::Intracomm)data; return *this;
  }
  // inter-language operability
  Intracomm& operator=(const MPI_Comm& data) {
    Comm::operator=(data);
    pmpi_comm = data;
    return *this;
  }

#else
  Intracomm(const Intracomm& data) : Comm(data.mpi_comm) { }

  inline Intracomm(const MPI_Comm& data);

  // assignment
  Intracomm& operator=(const Intracomm& data) {
    mpi_comm = data.mpi_comm; return *this;
  }

  Intracomm& operator=(const Comm_Null& data) {
    mpi_comm = data; return *this;
  }

  // inter-language operability
  Intracomm& operator=(const MPI_Comm& data) {
    mpi_comm = data; return *this; } 

#endif

  //
  // Collective Communication
  //

  virtual void
  Barrier() const;

  virtual void
  Bcast(void *buffer, int count, 
	const Datatype& datatype, int root) const;
  
  virtual void
  Gather(const void *sendbuf, int sendcount, 
	 const Datatype & sendtype, 
	 void *recvbuf, int recvcount, 
	 const Datatype & recvtype, int root) const;
  
  virtual void
  Gatherv(const void *sendbuf, int sendcount, 
	  const Datatype & sendtype, void *recvbuf, 
	  const int recvcounts[], const int displs[], 
	  const Datatype & recvtype, int root) const;
  
  virtual void
  Scatter(const void *sendbuf, int sendcount, 
	  const Datatype & sendtype, 
	  void *recvbuf, int recvcount, 
	  const Datatype & recvtype, int root) const;
  
  virtual void
  Scatterv(const void *sendbuf, const int sendcounts[], 
	   const int displs[], const Datatype & sendtype,
	   void *recvbuf, int recvcount, 
	   const Datatype & recvtype, int root) const;
  
  virtual void
  Allgather(const void *sendbuf, int sendcount, 
	    const Datatype & sendtype, void *recvbuf, 
	    int recvcount, const Datatype & recvtype) const;
  
  virtual void
  Allgatherv(const void *sendbuf, int sendcount, 
	     const Datatype & sendtype, void *recvbuf, 
	     const int recvcounts[], const int displs[],
	     const Datatype & recvtype) const;
  
  virtual void
  Alltoall(const void *sendbuf, int sendcount, 
	   const Datatype & sendtype, void *recvbuf, 
	   int recvcount, const Datatype & recvtype) const;
  
  virtual void
  Alltoallv(const void *sendbuf, const int sendcounts[], 
	    const int sdispls[], const Datatype & sendtype, 
	    void *recvbuf, const int recvcounts[], 
	    const int rdispls[], const Datatype & recvtype) const;
  
  virtual void
  Reduce(const void *sendbuf, void *recvbuf, int count, 
	 const Datatype & datatype, const Op & op, 
	 int root) const;
  
  
  virtual void
  Allreduce(const void *sendbuf, void *recvbuf, int count,
	    const Datatype & datatype, const Op & op) const;
  
  virtual void
  Reduce_scatter(const void *sendbuf, void *recvbuf, 
		 int recvcounts[], 
		 const Datatype & datatype, 
		 const Op & op) const;
  
  virtual void
  Scan(const void *sendbuf, void *recvbuf, int count, 
       const Datatype & datatype, const Op & op) const;
  
  Intracomm
  Dup() const;
  

  virtual
#if VIRTUAL_FUNC_RET
  Intracomm&
#else
  Comm&
#endif
  Clone() const;

  virtual Intracomm
  Create(const Group& group) const;
  
  virtual Intracomm
  Split(int color, int key) const;

  virtual Intercomm
  Create_intercomm(int local_leader, const Comm& peer_comm,
		   int remote_leader, int tag) const;
  
  virtual Cartcomm
  Create_cart(int ndims, const int dims[],
	      const MPI2CPP_BOOL_T periods[], MPI2CPP_BOOL_T reorder) const;
  
  virtual Graphcomm
  Create_graph(int nnodes, const int index[],
	       const int edges[], MPI2CPP_BOOL_T reorder) const;

  //#if _MPIPP_PROFILING_
  //  virtual const PMPI::Comm& get_pmpi_comm() const { return pmpi_comm; }
  //#endif
protected:


#if _MPIPP_PROFILING_
  PMPI::Intracomm pmpi_comm;
#endif

public: // JGS see above about friend decls
#if ! _MPIPP_PROFILING_
  static Op* current_op;
#endif

};
/* process-header: end of intracomm.h */
/* process-header: including topology.h */


class Cartcomm : public Intracomm {
public:

  // construction
  Cartcomm() { }
  // copy
  Cartcomm(const Comm_Null& data) : Intracomm(data) { }
  // inter-language operability
  inline Cartcomm(const MPI_Comm& data);
#if _MPIPP_PROFILING_
  Cartcomm(const Cartcomm& data) : Intracomm(data) { }
  Cartcomm(const PMPI::Cartcomm& d) : pmpi_comm(d), Intracomm((const PMPI::Intracomm&)d) { }
  
  // assignment
  Cartcomm& operator=(const Cartcomm& data) {
    Intracomm::operator=(data);
    pmpi_comm = data.pmpi_comm; return *this; }
  Cartcomm& operator=(const Comm_Null& data) {
    Intracomm::operator=(data);
    pmpi_comm = (PMPI::Cartcomm)data; return *this; }
  // inter-language operability
  Cartcomm& operator=(const MPI_Comm& data) {
    Intracomm::operator=(data);
    pmpi_comm = data; return *this; }
#else
  Cartcomm(const Cartcomm& data) : Intracomm(data.mpi_comm) { }
  // assignment
  Cartcomm& operator=(const Cartcomm& data) {
    mpi_comm = data.mpi_comm; return *this; }
  Cartcomm& operator=(const Comm_Null& data) {
    mpi_comm = data; return *this; }
  // inter-language operability
  Cartcomm& operator=(const MPI_Comm& data) {
    mpi_comm = data; return *this; } 
#endif
  //
  // Groups, Contexts, and Communicators
  //

  Cartcomm Dup() const;

  virtual
#if VIRTUAL_FUNC_RET
  Cartcomm&
#else
  Comm&
#endif
  Clone() const;


  //
  // Groups, Contexts, and Communicators
  //

  virtual int Get_dim() const;

  virtual void Get_topo(int maxdims, int dims[], MPI2CPP_BOOL_T periods[],
			int coords[]) const;

  virtual int Get_cart_rank(const int coords[]) const;

  virtual void Get_coords(int rank, int maxdims, int coords[]) const;

  virtual void Shift(int direction, int disp,
		     int &rank_source, int &rank_dest) const;
  
  virtual Cartcomm Sub(const MPI2CPP_BOOL_T remain_dims[]);

  virtual int Map(int ndims, const int dims[], const MPI2CPP_BOOL_T periods[]) const;

#if _MPIPP_PROFILING_
private:
  PMPI::Cartcomm pmpi_comm;
#endif
};


//===================================================================
//                    Class Graphcomm
//===================================================================

class Graphcomm : public Intracomm {
public:

  // construction
  Graphcomm() { }
  // copy
  Graphcomm(const Comm_Null& data) : Intracomm(data) { }
  // inter-language operability
  inline Graphcomm(const MPI_Comm& data);
#if _MPIPP_PROFILING_
  Graphcomm(const Graphcomm& data) : Intracomm(data) { }
  Graphcomm(const PMPI::Graphcomm& d) : pmpi_comm(d), Intracomm((const PMPI::Intracomm&)d) { }
  
  // assignment
  Graphcomm& operator=(const Graphcomm& data) {
    Intracomm::operator=(data);
    pmpi_comm = data.pmpi_comm; return *this; }
  Graphcomm& operator=(const Comm_Null& data) {
    Intracomm::operator=(data);
    pmpi_comm = (PMPI::Graphcomm)data; return *this; }
  // inter-language operability
  Graphcomm& operator=(const MPI_Comm& data) {
    Intracomm::operator=(data);
    pmpi_comm = data; return *this; }

#else
  Graphcomm(const Graphcomm& data) : Intracomm(data.mpi_comm) { }
  // assignment
  Graphcomm& operator=(const Graphcomm& data) {
    mpi_comm = data.mpi_comm; return *this; }
  Graphcomm& operator=(const Comm_Null& data) {
    mpi_comm = data; return *this; }
  // inter-language operability
  Graphcomm& operator=(const MPI_Comm& data) {
    mpi_comm = data; return *this; } 
#endif

  //
  // Groups, Contexts, and Communicators
  //
  
  Graphcomm Dup() const;

  virtual
#if VIRTUAL_FUNC_RET  
  Graphcomm&
#else
  Comm&
#endif
  Clone() const;

  //
  //  Process Topologies
  //

  virtual void Get_dims(int nnodes[], int nedges[]) const;

  virtual void Get_topo(int maxindex, int maxedges, int index[], 
			int edges[]) const;

  virtual int Get_neighbors_count(int rank) const;

  virtual void Get_neighbors(int rank, int maxneighbors, 
			     int neighbors[]) const;

  virtual int Map(int nnodes, const int index[], 
		  const int edges[]) const;

#if _MPIPP_PROFILING_
private:
  PMPI::Graphcomm pmpi_comm;
#endif
};

/* process-header: end of topology.h */
/* process-header: including intercomm.h */


class Intercomm : public Comm {
#if _MPIPP_PROFILING_
  //  friend class PMPI::Intercomm;
#endif
public:

  // construction
  Intercomm() : Comm(MPI_COMM_NULL) { }
  // copy
  Intercomm(const Comm_Null& data) : Comm(data) { }
  // inter-language operability
  Intercomm(const MPI_Comm& data) : Comm(data) { }

#if _MPIPP_PROFILING_
  // copy
  Intercomm(const Intercomm& data) : pmpi_comm(data.pmpi_comm), Comm(data) { }
  Intercomm(const PMPI::Intercomm& d) : pmpi_comm(d), Comm((const PMPI::Comm&)d) { }

  // assignment
  Intercomm& operator=(const Intercomm& data) {
    Comm::operator=(data);
    pmpi_comm = data.pmpi_comm; return *this; }
  Intercomm& operator=(const Comm_Null& data) {
    Comm::operator=(data);
    Intercomm& ic = (Intercomm&)data;
    pmpi_comm = ic.pmpi_comm; return *this; }
  // inter-language operability
  Intercomm& operator=(const MPI_Comm& data) {
    Comm::operator=(data);
    pmpi_comm = PMPI::Intercomm(data); return *this; }
#else
  // copy
  Intercomm(const Intercomm& data) : Comm(data.mpi_comm) { }
  // assignment
  Intercomm& operator=(const Intercomm& data) {
    mpi_comm = data.mpi_comm; return *this; }
  Intercomm& operator=(const Comm_Null& data) {
    mpi_comm = data; return *this; }
  // inter-language operability
  Intercomm& operator=(const MPI_Comm& data) {
    mpi_comm = data; return *this; } 

#endif
  

  //
  // Groups, Contexts, and Communicators
  //

  Intercomm Dup() const;

  virtual
#if VIRTUAL_FUNC_RET
  Intercomm&
#else
  Comm&
#endif
  Clone() const;

  virtual int Get_remote_size() const;

  virtual Group Get_remote_group() const;

  virtual Intracomm Merge(MPI2CPP_BOOL_T high);

  //#if _MPIPP_PROFILING_
  //  virtual const PMPI::Comm& get_pmpi_comm() const { return pmpi_comm; }
  //#endif

#if _MPIPP_PROFILING_
private:
  PMPI::Intercomm pmpi_comm;
#endif
};
/* process-header: end of intercomm.h */
  
#if ! _MPIPP_USENAMESPACE_
private:
  MPI() { }
#endif
};

#if _MPIPP_PROFILING_
/* process-header: including pop_inln.h */

inline PMPI::Op&
PMPI::Op::operator=(const PMPI::Op& op)
{
  mpi_op = op.mpi_op;
  op_user_function = op.op_user_function;
  return *this;
} 

// comparison
inline MPI2CPP_BOOL_T
PMPI::Op::operator==(const PMPI::Op &a)
{
  return (MPI2CPP_BOOL_T)(mpi_op == a.mpi_op);
}

inline MPI2CPP_BOOL_T //this will never be used?
PMPI::Op::operator!= (const PMPI::Op &a) { return (MPI2CPP_BOOL_T)!(*this == a); }

// inter-language operability
inline PMPI::Op&
PMPI::Op::operator= (const MPI_Op &i) { mpi_op = i; return *this; }

inline
PMPI::Op::operator MPI_Op () const { return mpi_op; }

//inline
//PMPI::Op::operator MPI_Op* () const { return (MPI_Op*)&mpi_op; }


/* process-header: end of pop_inln.h */
/* process-header: including pgroup_inln.h */



/* process-header: end of pgroup_inln.h */
/* process-header: including pstatus_inln.h */
/* process-header: end of pstatus_inln.h */
/* process-header: including prequest_inln.h */

/* process-header: end of prequest_inln.h */
#endif

//
// These are the "real" functions, whether prototyping is enabled
// or not. These functions are assigned to either the MPI::XXX class
// or the PMPI::XXX class based on the value of the macro _REAL_MPI_
// which is set in mpi2c++_config.h.
// If prototyping is enabled, there is a top layer that calls these
// PMPI functions, and this top layer is in the XXX.cc files.
//



/* process-header: including datatype_inln.h */

//
// Point-to-Point Communication
//

inline _REAL_MPI_::Datatype
_REAL_MPI_::Datatype::Create_contiguous(int count) const
{
  MPI_Datatype newtype;
  (void)MPI_Type_contiguous(count, mpi_datatype, &newtype);
  return newtype;
}

inline _REAL_MPI_::Datatype
_REAL_MPI_::Datatype::Create_vector(int count, int blocklength,
			     int stride) const
{
  MPI_Datatype newtype;
  (void)MPI_Type_vector(count, blocklength, stride, mpi_datatype, &newtype);
  return newtype;
}

inline _REAL_MPI_::Datatype
_REAL_MPI_::Datatype::Create_indexed(int count,
				     const int array_of_blocklengths[], 
				     const int array_of_displacements[]) const
{
  MPI_Datatype newtype;
  (void)MPI_Type_indexed(count, (int *) array_of_blocklengths, 
			 (int *) array_of_displacements, mpi_datatype, &newtype);
  return newtype;
}

// JGS MPI_TYPE_STRUCT soon to be depracated in favor of
// MPI_TYPE_CREATE_STRUCT
inline _REAL_MPI_::Datatype
_REAL_MPI_::Datatype::Create_struct(int count, const int array_of_blocklengths[],
				    const _REAL_MPI_::Aint array_of_displacements[],
				    const _REAL_MPI_::Datatype array_of_types[])
{
  MPI_Datatype newtype;
  int i;
  MPI_Datatype* type_array = new MPI_Datatype[count];
  for (i=0; i < count; i++)
    type_array[i] = array_of_types[i];

  (void)MPI_Type_struct(count, (int*)array_of_blocklengths,
			(MPI_Aint*)array_of_displacements, type_array, &newtype);
  delete [] type_array;
  return newtype;
}

//JGS MPI_Type_hindexed to be replaced by MPI_Type_create_hindexed
inline _REAL_MPI_::Datatype
_REAL_MPI_::Datatype::Create_hindexed(int count, const int array_of_blocklengths[],
				      const _REAL_MPI_::Aint array_of_displacements[]) const
{
  MPI_Datatype newtype;
  (void)MPI_Type_hindexed(count, (int*)array_of_blocklengths,
			  (MPI_Aint*)array_of_displacements,
			  mpi_datatype, &newtype) ;
  return newtype;
}

//JGS MPI_Type_hvector to be replaced by MPI_Type_create_hvector
inline _REAL_MPI_::Datatype
_REAL_MPI_::Datatype::Create_hvector(int count, int blocklength,
				     _REAL_MPI_::Aint stride) const
{
  MPI_Datatype newtype;
  (void)MPI_Type_hvector(count, blocklength, (MPI_Aint)stride,
			 mpi_datatype, &newtype);

  return newtype;
}

inline int
_REAL_MPI_::Datatype::Get_size() const 
{
  int size;
  (void)MPI_Type_size(mpi_datatype, &size);
  return size;
}

inline void
_REAL_MPI_::Datatype::Get_extent(_REAL_MPI_::Aint& lb, _REAL_MPI_::Aint& extent) const
{
  // JGS MPI_TYPE_EXTENT and MPI_LB soon to be deprecated
  // in favor of MPI_TYPE_GET_EXTENT
  (void)MPI_Type_lb(mpi_datatype, &lb);
  (void)MPI_Type_extent(mpi_datatype, &extent); 
}

inline void
_REAL_MPI_::Datatype::Commit() 
{
  (void)MPI_Type_commit(&mpi_datatype);
}

inline void
_REAL_MPI_::Datatype::Free()
{
  (void)MPI_Type_free(&mpi_datatype);
}

inline void
_REAL_MPI_::Datatype::Pack(const void* inbuf, int incount,
			   void *outbuf, int outsize,
			   int& position, const _REAL_MPI_::Comm &comm) const
{
  (void)MPI_Pack((void *) inbuf, incount,  mpi_datatype, outbuf,
		 outsize, &position, comm);
}

inline void
_REAL_MPI_::Datatype::Unpack(const void* inbuf, int insize,
			     void *outbuf, int outcount, int& position,
			     const _REAL_MPI_::Comm& comm) const 
{
  (void)MPI_Unpack((void *) inbuf, insize, &position,
		   outbuf, outcount, mpi_datatype, comm);
}

inline int
_REAL_MPI_::Datatype::Pack_size(int incount, const _REAL_MPI_::Comm& comm) const 
{
  int size;
  (void)MPI_Pack_size(incount, mpi_datatype, comm, &size);
  return size;
}
/* process-header: end of datatype_inln.h */
/* process-header: including functions_inln.h */


//
// Point-to-Point Communication
//

inline void 
_REAL_MPI_::Attach_buffer(void* buffer, int size)
{
  (void)MPI_Buffer_attach(buffer, size);
}

inline int 
_REAL_MPI_::Detach_buffer(void*& buffer)
{
  int size;
  (void)MPI_Buffer_detach(&buffer, &size);
  return size;
}

//
// Process Topologies
//

inline void
_REAL_MPI_::Compute_dims(int nnodes, int ndims, int dims[])
{
  (void)MPI_Dims_create(nnodes, ndims, dims);
}


//
// Environmental Inquiry
//

inline void 
_REAL_MPI_::Get_processor_name(char*& name, int& resultlen)
{
  (void)MPI_Get_processor_name(name, &resultlen);
}

inline void
_REAL_MPI_::Get_error_string(int errorcode, char*& string, int& resultlen)
{
  (void)MPI_Error_string(errorcode, string, &resultlen);
}

inline int 
_REAL_MPI_::Get_error_class(int errorcode) 
{
  int errorclass;
  (void)MPI_Error_class(errorcode, &errorclass);
  return errorclass;
}

inline double 
_REAL_MPI_::Wtime()
{
  return (MPI_Wtime());
}

inline double 
_REAL_MPI_::Wtick()
{
  return (MPI_Wtick());
}

inline void
_REAL_MPI_::Real_init()
{
  MPI::ERRORS_THROW_EXCEPTIONS.init();
}

inline void
_REAL_MPI_::Init(int& argc, char**& argv)
{
  (void)MPI_Init(&argc, &argv);
  Real_init();
}

inline void
_REAL_MPI_::Init()
{
  (void)MPI_Init(0, 0);
  Real_init();
}

inline void
_REAL_MPI_::Finalize()
{
  (void)MPI_Finalize();
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Is_initialized()
{
  int t;
  (void)MPI_Initialized(&t);
  return (MPI2CPP_BOOL_T) t;
}

//
// Profiling
//

inline void
_REAL_MPI_::Pcontrol(const int level, ...)
{
  va_list ap;
  va_start(ap, level);
 
  (void)MPI_Pcontrol(level, ap);
  va_end(ap);
}

//JGS, MPI_Address soon to be replaced by MPI_Get_address
inline _REAL_MPI_::Aint
_REAL_MPI_::Get_address(void* location)
{
  _REAL_MPI_::Aint ret;
  MPI_Address(location, &ret);
  return ret;
}


/* process-header: end of functions_inln.h */
/* process-header: including request_inln.h */

//
// Point-to-Point Communication
//

inline void
_REAL_MPI_::Request::Wait(_REAL_MPI_::Status &status) 
{
  (void)MPI_Wait(&mpi_request, &status.mpi_status);
}

inline void
_REAL_MPI_::Request::Wait() 
{
  (void)MPI_Wait(&mpi_request, &ignored_status.mpi_status);
}

inline void
_REAL_MPI_::Request::Free() 
{
  (void)MPI_Request_free(&mpi_request);
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Request::Test(_REAL_MPI_::Status &status) 
{
  int t;
  (void)MPI_Test(&mpi_request, &t, &status.mpi_status);
  return (MPI2CPP_BOOL_T) t;
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Request::Test() 
{
  int t;
  (void)MPI_Test(&mpi_request, &t, &ignored_status.mpi_status);
  return (MPI2CPP_BOOL_T) t;
}

inline int
_REAL_MPI_::Request::Waitany(int count, _REAL_MPI_::Request array[],
			     _REAL_MPI_::Status& status)
{
  int index, i;
  MPI_Request* array_of_requests = new MPI_Request[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = array[i];
  (void)MPI_Waitany(count, array_of_requests, &index, &status.mpi_status);
  for (i=0; i < count; i++)
    array[i] = array_of_requests[i];
  delete [] array_of_requests;
  return index;
}

inline int
_REAL_MPI_::Request::Waitany(int count, _REAL_MPI_::Request array[])
{
  int index, i;
  MPI_Request* array_of_requests = new MPI_Request[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = array[i];
  (void)MPI_Waitany(count, array_of_requests, &index, &ignored_status.mpi_status);
  for (i=0; i < count; i++)
    array[i] = array_of_requests[i];
  delete [] array_of_requests;
  return index; //JGS, Waitany return value
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Request::Testany(int count, _REAL_MPI_::Request array[],
			     int& index, _REAL_MPI_::Status& status)
{
  int i, flag;
  MPI_Request* array_of_requests = new MPI_Request[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = array[i];
  (void)MPI_Testany(count, array_of_requests, &index, &flag, &status.mpi_status);
  for (i=0; i < count; i++)
    array[i] = array_of_requests[i];
  delete [] array_of_requests;
  return (MPI2CPP_BOOL_T)flag;
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Request::Testany(int count, _REAL_MPI_::Request array[], int& index)
{
  int i, flag;
  MPI_Request* array_of_requests = new MPI_Request[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = array[i];
  (void)MPI_Testany(count, array_of_requests, &index, &flag, &ignored_status.mpi_status);
  for (i=0; i < count; i++)
    array[i] = array_of_requests[i];
  delete [] array_of_requests;
  return (MPI2CPP_BOOL_T)flag;
}

inline void
_REAL_MPI_::Request::Waitall(int count, _REAL_MPI_::Request req_array[],
			     _REAL_MPI_::Status stat_array[])
{
  int i;
  MPI_Request* array_of_requests = new MPI_Request[count];
  MPI_Status* array_of_statuses = new MPI_Status[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Waitall(count, array_of_requests, array_of_statuses);
  for (i=0; i < count; i++)
    req_array[i] = array_of_requests[i];
  for (i=0; i < count; i++)
    stat_array[i] = array_of_statuses[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
}

inline void
_REAL_MPI_::Request::Waitall(int count, _REAL_MPI_::Request req_array[])
{
  int i;
  MPI_Request* array_of_requests = new MPI_Request[count];
  MPI_Status* array_of_statuses = new MPI_Status[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Waitall(count, array_of_requests, array_of_statuses);
  for (i=0; i < count; i++)
    req_array[i] = array_of_requests[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
} 

inline MPI2CPP_BOOL_T
_REAL_MPI_::Request::Testall(int count, _REAL_MPI_::Request req_array[],
			     _REAL_MPI_::Status stat_array[])
{
  int i, flag;
  MPI_Request* array_of_requests = new MPI_Request[count];
  MPI_Status* array_of_statuses = new MPI_Status[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Testall(count, array_of_requests, &flag, array_of_statuses);
  for (i=0; i < count; i++)
    req_array[i] = array_of_requests[i];
  for (i=0; i < count; i++)
    stat_array[i] = array_of_statuses[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
  return (MPI2CPP_BOOL_T) flag;
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Request::Testall(int count, _REAL_MPI_::Request req_array[])
{
  int i, flag;
  MPI_Request* array_of_requests = new MPI_Request[count];
  MPI_Status* array_of_statuses = new MPI_Status[count];
  for (i=0; i < count; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Testall(count, array_of_requests, &flag, array_of_statuses);
  for (i=0; i < count; i++)
    req_array[i] = array_of_requests[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
  return (MPI2CPP_BOOL_T) flag;
} 

inline int
_REAL_MPI_::Request::Waitsome(int incount, _REAL_MPI_::Request req_array[],
			      int array_of_indices[], _REAL_MPI_::Status stat_array[]) 
{
  int i, outcount;
  MPI_Request* array_of_requests = new MPI_Request[incount];
  MPI_Status* array_of_statuses = new MPI_Status[incount];
  for (i=0; i < incount; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Waitsome(incount, array_of_requests, &outcount,
		     array_of_indices, array_of_statuses);
  for (i=0; i < incount; i++)
    req_array[i] = array_of_requests[i];
  for (i=0; i < incount; i++)
    stat_array[i] = array_of_statuses[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
  return outcount;
}

inline int
_REAL_MPI_::Request::Waitsome(int incount, _REAL_MPI_::Request req_array[],
			      int array_of_indices[]) 
{
  int i, outcount;
  MPI_Request* array_of_requests = new MPI_Request[incount];
  MPI_Status* array_of_statuses = new MPI_Status[incount];
  for (i=0; i < incount; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Waitsome(incount, array_of_requests, &outcount,
		     array_of_indices, array_of_statuses);
  for (i=0; i < incount; i++)
    req_array[i] = array_of_requests[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
  return outcount;
}

inline int
_REAL_MPI_::Request::Testsome(int incount, _REAL_MPI_::Request req_array[],
			      int array_of_indices[], _REAL_MPI_::Status stat_array[]) 
{
  int i, outcount;
  MPI_Request* array_of_requests = new MPI_Request[incount];
  MPI_Status* array_of_statuses = new MPI_Status[incount];
  for (i=0; i < incount; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Testsome(incount, array_of_requests, &outcount,
		     array_of_indices, array_of_statuses);
  for (i=0; i < incount; i++)
    req_array[i] = array_of_requests[i];
  for (i=0; i < incount; i++)
    stat_array[i] = array_of_statuses[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
  return outcount;
}

inline int
_REAL_MPI_::Request::Testsome(int incount, _REAL_MPI_::Request req_array[],
			      int array_of_indices[]) 
{
  int i, outcount;
  MPI_Request* array_of_requests = new MPI_Request[incount];
  MPI_Status* array_of_statuses = new MPI_Status[incount];
  for (i=0; i < incount; i++)
    array_of_requests[i] = req_array[i];
  (void)MPI_Testsome(incount, array_of_requests, &outcount,
		     array_of_indices, array_of_statuses);
  for (i=0; i < incount; i++)
    req_array[i] = array_of_requests[i];
  delete [] array_of_requests;
  delete [] array_of_statuses;
  return outcount;
}

inline void
_REAL_MPI_::Request::Cancel(void) const
{
  (void)MPI_Cancel((MPI_Request*)&mpi_request);
}

inline void
_REAL_MPI_::Prequest::Start()
{
  (void)MPI_Start(&mpi_request);
}

inline void
_REAL_MPI_::Prequest::Startall(int count, _REAL_MPI_:: Prequest array_of_requests[])
{
  //convert the array of Prequests to an array of MPI_requests
  MPI_Request* mpi_requests = new MPI_Request[count];
  int i;
  for (i=0; i < count; i++) {
    mpi_requests[i] = array_of_requests[i];
  }
  (void)MPI_Startall(count, mpi_requests); 
  for (i=0; i < count; i++)
    array_of_requests[i].mpi_request = mpi_requests[i] ;
  delete [] mpi_requests;
} 

/* process-header: end of request_inln.h */
/* process-header: including comm_inln.h */
 
//
// Point-to-Point
//

inline void
_REAL_MPI_::Comm::Send(const void *buf, int count, 
		const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  (void)MPI_Send((void *)buf, count, datatype, dest, tag, mpi_comm);
}

inline void
_REAL_MPI_::Comm::Recv(void *buf, int count, const _REAL_MPI_::Datatype & datatype,
		int source, int tag, _REAL_MPI_::Status & status) const
{
  (void)MPI_Recv(buf, count, datatype, source, tag, mpi_comm, &status.mpi_status);
}

inline void
_REAL_MPI_::Comm::Recv(void *buf, int count, const _REAL_MPI_::Datatype & datatype,
				    int source, int tag) const
{
  (void)MPI_Recv(buf, count, datatype, source, 
		 tag, mpi_comm, &ignored_status.mpi_status);
}

inline void
_REAL_MPI_::Comm::Bsend(const void *buf, int count,
		 const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  (void)MPI_Bsend((void *)buf, count, datatype, 
		  dest, tag, mpi_comm);
}

inline void
_REAL_MPI_::Comm::Ssend(const void *buf, int count, 
		 const _REAL_MPI_::Datatype & datatype, int dest, int tag) const 
{
  (void)MPI_Ssend((void *)buf, count,  datatype, dest, 
		  tag, mpi_comm);
}

inline void
_REAL_MPI_::Comm::Rsend(const void *buf, int count,
		 const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  (void)MPI_Rsend((void *)buf, count, datatype, 
		  dest, tag, mpi_comm);
}

inline _REAL_MPI_::Request
_REAL_MPI_::Comm::Isend(const void *buf, int count,
		 const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  MPI_Request request;
  (void)MPI_Isend((void *)buf, count, datatype, 
		  dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Request
_REAL_MPI_::Comm::Ibsend(const void *buf, int count,
		  const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  MPI_Request request;    
  (void)MPI_Ibsend((void *)buf, count, datatype, 
		   dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Request
_REAL_MPI_::Comm::Issend(const void *buf, int count,
		  const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  MPI_Request request;        
  (void)MPI_Issend((void *)buf, count, datatype,
		   dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Request
_REAL_MPI_::Comm::Irsend(const void *buf, int count,
		  const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  MPI_Request request;
  (void)MPI_Irsend((void *) buf, count, datatype, 
		   dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Request
_REAL_MPI_::Comm::Irecv(void *buf, int count,
		 const _REAL_MPI_::Datatype & datatype, int source, int tag) const
{
  MPI_Request request;
  (void)MPI_Irecv(buf, count, datatype, source, 
		  tag, mpi_comm, &request);
  return request;
}


inline MPI2CPP_BOOL_T
_REAL_MPI_::Comm::Iprobe(int source, int tag, _REAL_MPI_::Status & status) const
{
  int t;
  (void)MPI_Iprobe(source, tag, mpi_comm, &t, &status.mpi_status);
  return (MPI2CPP_BOOL_T) t;
}
  
inline MPI2CPP_BOOL_T
_REAL_MPI_::Comm::Iprobe(int source, int tag) const
{
  int t;
  (void)MPI_Iprobe(source, tag, mpi_comm, &t, &ignored_status.mpi_status);
  return (MPI2CPP_BOOL_T) t;
}

inline void
_REAL_MPI_::Comm::Probe(int source, int tag, _REAL_MPI_::Status & status) const
{
  (void)MPI_Probe(source, tag, mpi_comm, &status.mpi_status);
}

inline void
_REAL_MPI_::Comm::Probe(int source, int tag) const
{
  (void)MPI_Probe(source, tag, mpi_comm, &ignored_status.mpi_status);  
}

inline _REAL_MPI_::Prequest
_REAL_MPI_::Comm::Send_init(const void *buf, int count,
		     const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{ 
  MPI_Request request;
  (void)MPI_Send_init((void *)buf, count, datatype, 
		      dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Prequest
_REAL_MPI_::Comm::Bsend_init(const void *buf, int count,
		      const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  MPI_Request request; 
  (void)MPI_Bsend_init((void *)buf, count, datatype, 
		       dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Prequest
_REAL_MPI_::Comm::Ssend_init(const void *buf, int count,
		      const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  MPI_Request request; 
  (void)MPI_Ssend_init((void *)buf, count, datatype,
		       dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Prequest
_REAL_MPI_::Comm::Rsend_init(const void *buf, int count,
		      const _REAL_MPI_::Datatype & datatype, int dest, int tag) const
{
  MPI_Request request; 
  (void)MPI_Rsend_init((void *)buf, count,  datatype,
		       dest, tag, mpi_comm, &request);
  return request;
}

inline _REAL_MPI_::Prequest
_REAL_MPI_::Comm::Recv_init(void *buf, int count,
		     const _REAL_MPI_::Datatype & datatype, int source, int tag) const
{
  MPI_Request request; 
  (void)MPI_Recv_init(buf, count, datatype, source, 
		      tag, mpi_comm, &request);
  return request;
}

inline void
_REAL_MPI_::Comm::Sendrecv(const void *sendbuf, int sendcount,
		    const _REAL_MPI_::Datatype & sendtype, int dest, int sendtag, 
		    void *recvbuf, int recvcount, 
		    const _REAL_MPI_::Datatype & recvtype, int source,
		    int recvtag, _REAL_MPI_::Status & status) const
{
  (void)MPI_Sendrecv((void *)sendbuf, sendcount, 
		     sendtype,
		     dest, sendtag, recvbuf, recvcount, 
		     recvtype, 
		     source, recvtag, mpi_comm, &status.mpi_status);
}

inline void
_REAL_MPI_::Comm::Sendrecv(const void *sendbuf, int sendcount,
		    const _REAL_MPI_::Datatype & sendtype, int dest, int sendtag, 
		    void *recvbuf, int recvcount, 
		    const _REAL_MPI_::Datatype & recvtype, int source,
		    int recvtag) const
{
  (void)MPI_Sendrecv((void *)sendbuf, sendcount, 
		     sendtype,
		     dest, sendtag, recvbuf, recvcount, 
		     recvtype, 
		     source, recvtag, mpi_comm, &ignored_status.mpi_status);
}

inline void
_REAL_MPI_::Comm::Sendrecv_replace(void *buf, int count,
			    const _REAL_MPI_::Datatype & datatype, int dest, 
			    int sendtag, int source,
			    int recvtag, _REAL_MPI_::Status & status) const 
{
  (void)MPI_Sendrecv_replace(buf, count, datatype, dest,
			     sendtag, source, recvtag, mpi_comm,
			     &status.mpi_status);
}

inline void
_REAL_MPI_::Comm::Sendrecv_replace(void *buf, int count,
			    const _REAL_MPI_::Datatype & datatype, int dest, 
			    int sendtag, int source,
			    int recvtag) const 
{
  (void)MPI_Sendrecv_replace(buf, count, datatype, dest,
			     sendtag, source, recvtag, mpi_comm,
			     &ignored_status.mpi_status);    
}

//
// Groups, Contexts, and Communicators
//

inline _REAL_MPI_::Group
_REAL_MPI_::Comm::Get_group() const 
{
  MPI_Group group;
  (void)MPI_Comm_group(mpi_comm, &group);
  return group;
}
  
inline int
_REAL_MPI_::Comm::Get_size() const 
{
  int size;
  (void)MPI_Comm_size (mpi_comm, &size);
  return size;
}
  
inline int
_REAL_MPI_::Comm::Get_rank() const 
{
  int rank;
  (void)MPI_Comm_rank (mpi_comm, &rank);
  return rank;
}
  
inline int
_REAL_MPI_::Comm::Compare(const _REAL_MPI_::Comm & comm1,
		   const _REAL_MPI_::Comm & comm2)
{
  int result;
  (void)MPI_Comm_compare(comm1, comm2, &result);
  return result;
}

inline void
_REAL_MPI_::Comm::Free(void) 
{
  (void)MPI_Comm_free(&mpi_comm);
  _REAL_MPI_::Comm::mpi_comm_map.erase((void*)mpi_comm);
}
  
inline MPI2CPP_BOOL_T
_REAL_MPI_::Comm::Is_inter() const
{
  int t;
  (void)MPI_Comm_test_inter(mpi_comm, &t);
  return (MPI2CPP_BOOL_T) t;
}
  
//
//Process Topologies
//

inline int
_REAL_MPI_::Comm::Get_topology() const 
{
  int status;
  (void)MPI_Topo_test(mpi_comm, &status);
  return status;
}
  
//
// Environmental Inquiry
//

inline void
_REAL_MPI_::Comm::Abort(int errorcode) 
{
  (void)MPI_Abort(mpi_comm, errorcode);
}

//
//  These C++ bindings are for MPI-2.
//  The MPI-1.2 functions called below are all
//  going to be deprecated and replaced in MPI-2.
//

inline void
_REAL_MPI_::Comm::Set_errhandler(const _REAL_MPI_::Errhandler& errhandler)
{
  my_errhandler = (_REAL_MPI_::Errhandler *)&errhandler;
  _REAL_MPI_::Comm::mpi_comm_map[(void*)mpi_comm] = this;
  (void)MPI_Errhandler_set(mpi_comm, errhandler);
}

inline _REAL_MPI_::Errhandler
_REAL_MPI_::Comm::Get_errhandler() const
{
  return *my_errhandler;
}

inline _REAL_MPI_::Errhandler
_REAL_MPI_::Comm::Create_errhandler(_REAL_MPI_::Comm::ERRHANDLERFN* function)
{
  MPI_Errhandler errhandler;
  (void)MPI_Errhandler_create(errhandler_intercept, &errhandler);
  _REAL_MPI_::Errhandler temp(errhandler);
  temp.handler_fn = (void(*)(_REAL_MPI_::Comm&, int*, ...))function;
  return temp;
}

//JGS I took the const out because it causes problems when trying to
//call this function with the predefined NULL_COPY_FN etc.
inline int
_REAL_MPI_::Comm::Create_keyval(_REAL_MPI_::Comm::COPYATTRFN* comm_copy_attr_fn,
				_REAL_MPI_::Comm::DELETEATTRFN* comm_delete_attr_fn,
				void* extra_state)
{
  int keyval;
  (void)MPI_Keyval_create(copy_attr_intercept, delete_attr_intercept,
			  &keyval, extra_state);
  MPI_SGI_Map::Pair* copy_and_delete = new MPI_SGI_Map::Pair((void*)comm_copy_attr_fn, (void*)comm_delete_attr_fn); 
  _REAL_MPI_::Comm::key_fn_map[(MPI_SGI_Map::address)keyval] = copy_and_delete;
  return keyval;
}

inline void
_REAL_MPI_::Comm::Free_keyval(int& comm_keyval)
{
  (void)MPI_Keyval_free(&comm_keyval);
  _REAL_MPI_::Comm::key_fn_map.erase((void*)comm_keyval);
}

inline void
_REAL_MPI_::Comm::Set_attr(int comm_keyval, const void* attribute_val) const
{
  CommType type;
  int status;

  (void)MPI_Comm_test_inter(mpi_comm, &status);
  if (status) {
    type = eIntercomm;
  }
  else {
    (void)MPI_Topo_test(mpi_comm, &status);    
    if (status == MPI_CART)
      type = eCartcomm;
    else if (status == MPI_GRAPH)
      type = eGraphcomm;
    else
      type = eIntracomm;
  }
  MPI_SGI_Map::Pair* comm_type = new MPI_SGI_Map::Pair((void*)this, (void*)type);
  _REAL_MPI_::Comm::mpi_comm_map[(MPI_SGI_Map::address)mpi_comm] = (MPI_SGI_Map::address)comm_type;
  (void)MPI_Attr_put(mpi_comm, comm_keyval, (void*)attribute_val);
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Comm::Get_attr(int comm_keyval, void* attribute_val) const
{
  int flag;
  (void)MPI_Attr_get(mpi_comm, comm_keyval, attribute_val, &flag);
  return (MPI2CPP_BOOL_T)flag;
}

inline void
_REAL_MPI_::Comm::Delete_attr(int comm_keyval)
{
  (void)MPI_Attr_delete(mpi_comm, comm_keyval);
}

inline int
_REAL_MPI_::Comm::NULL_COPY_FN(const _REAL_MPI_::Comm& oldcomm, int comm_keyval,
			       void* extra_state, void* attribute_val_in,
			       void* attribute_val_out, MPI2CPP_BOOL_T& flag)
{
#if MPI2CPP_IBM_SP
  //SP2 does not implement this function
  flag = false;
  return MPI_SUCCESS;
#else

#if _MPIPP_BOOL_NE_INT_
  int f = (int)flag;
  int ret;
  if (MPI_NULL_COPY_FN != 0) {
    ret = MPI_NULL_COPY_FN(oldcomm, comm_keyval, extra_state, attribute_val_in,
			   attribute_val_out, &f);
    flag = (MPI2CPP_BOOL_T)f;
  } else {
    ret = MPI_SUCCESS;
    flag = true;
  }
  return ret;
#else
  if (MPI_NULL_COPY_FN != 0)
    return MPI_NULL_COPY_FN(oldcomm, comm_keyval, extra_state, 
			    attribute_val_in, attribute_val_out, (int*)&flag);
  else
    return MPI_SUCCESS;
#endif

#endif
}

inline int
_REAL_MPI_::Comm::DUP_FN(const _REAL_MPI_::Comm& oldcomm, int comm_keyval,
			 void* extra_state, void* attribute_val_in,
			 void* attribute_val_out, MPI2CPP_BOOL_T& flag)
{
#if MPI2CPP_IBM_SP
  flag = false;
  return 0;
#else
#if _MPIPP_BOOL_NE_INT_
  int f = (int)flag;
  int ret;
  ret = MPI_DUP_FN(oldcomm, comm_keyval, extra_state, attribute_val_in,
		   attribute_val_out, &f);
  flag = (MPI2CPP_BOOL_T) f;
  return ret;
#else
  return MPI_DUP_FN(oldcomm, comm_keyval, extra_state, attribute_val_in,
		    attribute_val_out, (int*)&flag);
#endif
#endif
}

inline int
_REAL_MPI_::Comm::NULL_DELETE_FN(_REAL_MPI_::Comm& comm, int comm_keyval, void* attribute_val,
				 void* extra_state)
{
#if MPI2CPP_IBM_SP
  return MPI_SUCCESS;
#else
  if (MPI_NULL_DELETE_FN != 0)
    return MPI_NULL_DELETE_FN(comm, comm_keyval, attribute_val, extra_state);
  else
    return MPI_SUCCESS;
#endif
}

/* process-header: end of comm_inln.h */
/* process-header: including intracomm_inln.h */

inline
_REAL_MPI_::Intracomm::Intracomm(const MPI_Comm& data) {
  int flag;
  if (MPI::Is_initialized() && (data != MPI_COMM_NULL)) {
    (void)MPI_Comm_test_inter(data, &flag);
    if (flag)
      mpi_comm = MPI_COMM_NULL;
    else
      mpi_comm = data;
  }
  else {
    mpi_comm = data;
  }
}

//
// Collective Communication
//

inline void
_REAL_MPI_::Intracomm::Barrier() const
{
  (void)MPI_Barrier(mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Bcast(void *buffer, int count, 
      const _REAL_MPI_::Datatype& datatype, int root) const
{ 
  (void)MPI_Bcast(buffer, count, datatype, root, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Gather(const void *sendbuf, int sendcount, 
			      const _REAL_MPI_::Datatype & sendtype, 
			      void *recvbuf, int recvcount, 
			      const _REAL_MPI_::Datatype & recvtype, int root) const
{
  (void)MPI_Gather((void *)sendbuf, sendcount, sendtype,
		   recvbuf, recvcount, recvtype, root, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Gatherv(const void *sendbuf, int sendcount, 
	const _REAL_MPI_::Datatype & sendtype, void *recvbuf, 
	const int recvcounts[], const int displs[], 
	const _REAL_MPI_::Datatype & recvtype, int root) const
{
  (void)MPI_Gatherv((void *)sendbuf, sendcount,  sendtype,
		    recvbuf, (int *)recvcounts, (int *)displs, 
		    recvtype, root, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Scatter(const void *sendbuf, int sendcount, 
	const _REAL_MPI_::Datatype & sendtype, 
	void *recvbuf, int recvcount, 
	const _REAL_MPI_::Datatype & recvtype, int root) const
{ 
  (void)MPI_Scatter((void *)sendbuf, sendcount, sendtype,
		    recvbuf, recvcount, recvtype, root, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Scatterv(const void *sendbuf, const int sendcounts[], 
	 const int displs[], const _REAL_MPI_::Datatype & sendtype,
	 void *recvbuf, int recvcount, 
	 const _REAL_MPI_::Datatype & recvtype, int root) const
{
  (void)MPI_Scatterv((void *)sendbuf, (int *) sendcounts, 
		     (int *) displs, sendtype, 
		     recvbuf, recvcount, recvtype, 
		     root, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Allgather(const void *sendbuf, int sendcount, 
	  const _REAL_MPI_::Datatype & sendtype, void *recvbuf, 
	  int recvcount, const _REAL_MPI_::Datatype & recvtype) const 
{
  (void)MPI_Allgather((void *) sendbuf, sendcount, 
		      sendtype, recvbuf, recvcount,
		      recvtype, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Allgatherv(const void *sendbuf, int sendcount, 
	   const _REAL_MPI_::Datatype & sendtype, void *recvbuf, 
	   const int recvcounts[], const int displs[],
	   const _REAL_MPI_::Datatype & recvtype) const
{
  (void)MPI_Allgatherv((void *)sendbuf, sendcount, 
		       sendtype, recvbuf, 
		       (int *) recvcounts, (int *) displs, 
		       recvtype, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Alltoall(const void *sendbuf, int sendcount, 
	 const _REAL_MPI_::Datatype & sendtype, void *recvbuf, 
	 int recvcount, const _REAL_MPI_::Datatype & recvtype) const
{
  (void)MPI_Alltoall((void *) sendbuf, sendcount,
		     sendtype, recvbuf, recvcount,
		     recvtype, mpi_comm);
}

inline void
_REAL_MPI_::Intracomm::Alltoallv(const void *sendbuf, const int sendcounts[], 
	  const int sdispls[], const _REAL_MPI_::Datatype & sendtype, 
	  void *recvbuf, const int recvcounts[], 
	  const int rdispls[], const _REAL_MPI_::Datatype & recvtype) const 
{
    (void)MPI_Alltoallv((void *) sendbuf, (int *) sendcounts, 
			(int *) sdispls, sendtype, recvbuf, 
			(int *) recvcounts, (int *) rdispls, 
			recvtype,mpi_comm);
}


inline void
_REAL_MPI_::Intracomm::Reduce(const void *sendbuf, void *recvbuf, int count, 
       const _REAL_MPI_::Datatype & datatype, const _REAL_MPI_::Op& op, 
       int root) const
{
  current_op = (_REAL_MPI_::Op*)&op;
  (void)MPI_Reduce((void*)sendbuf, recvbuf, count, datatype, op, root, mpi_comm);
  current_op = (Op*)0;
}

inline void
_REAL_MPI_::Intracomm::Allreduce(const void *sendbuf, void *recvbuf, int count,
	  const _REAL_MPI_::Datatype & datatype, const _REAL_MPI_::Op& op) const
{
  current_op = (_REAL_MPI_::Op*)&op;
  (void)MPI_Allreduce ((void*)sendbuf, recvbuf, count, datatype,  op, mpi_comm);
  current_op = (Op*)0;
}

inline void
_REAL_MPI_::Intracomm::Reduce_scatter(const void *sendbuf, void *recvbuf, 
	       int recvcounts[], 
	       const _REAL_MPI_::Datatype & datatype, 
	       const _REAL_MPI_::Op& op) const
{
  current_op = (_REAL_MPI_::Op*)&op;
  (void)MPI_Reduce_scatter((void*)sendbuf, recvbuf, recvcounts,
			   datatype, op, mpi_comm);
  current_op = (Op*)0;
}

inline void
_REAL_MPI_::Intracomm::Scan(const void *sendbuf, void *recvbuf, int count, 
     const _REAL_MPI_::Datatype & datatype, const _REAL_MPI_::Op& op) const
{
  current_op = (_REAL_MPI_::Op*)&op;
  (void)MPI_Scan((void *)sendbuf, recvbuf, count, datatype, op, mpi_comm);
  current_op = (Op*)0;
}

inline _REAL_MPI_::Intracomm
_REAL_MPI_::Intracomm::Dup() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  return newcomm;
}

#if VIRTUAL_FUNC_RET
inline _REAL_MPI_::Intracomm&
_REAL_MPI_::Intracomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Intracomm* dup = new _REAL_MPI_::Intracomm(newcomm);
  return *dup;
}
#else
inline _REAL_MPI_::Comm&
_REAL_MPI_::Intracomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Intracomm* dup = new _REAL_MPI_::Intracomm(newcomm);
  return *dup;
}
#endif

inline _REAL_MPI_::Intracomm
_REAL_MPI_::Intracomm::Create(const _REAL_MPI_::Group& group) const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_create(mpi_comm, group, &newcomm);
  return newcomm;
}

inline _REAL_MPI_::Intracomm
_REAL_MPI_::Intracomm::Split(int color, int key) const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_split(mpi_comm, color, key, &newcomm);
  return newcomm;
}



inline _REAL_MPI_::Intercomm
_REAL_MPI_::Intracomm::Create_intercomm(int local_leader,
					const _REAL_MPI_::Comm& peer_comm,
					int remote_leader, int tag) const
{
  MPI_Comm newintercomm;
  (void)MPI_Intercomm_create(mpi_comm, local_leader, peer_comm,
			     remote_leader, tag, &newintercomm); 
  return newintercomm;
}

inline _REAL_MPI_::Cartcomm
_REAL_MPI_::Intracomm::Create_cart(int ndims, const int dims[],
				   const MPI2CPP_BOOL_T periods[], MPI2CPP_BOOL_T reorder) const
{
  int *int_periods = new int [ndims];
  for (int i=0; i<ndims; i++)
    int_periods[i] = (int) periods[i];
  
  MPI_Comm newcomm;
  (void)MPI_Cart_create(mpi_comm, ndims, (int*)dims, 
		      int_periods, (int)reorder, &newcomm);
  delete [] int_periods;
  return newcomm;
}

inline _REAL_MPI_::Graphcomm
_REAL_MPI_::Intracomm::Create_graph(int nnodes, const int index[],
				    const int edges[], MPI2CPP_BOOL_T reorder) const
{
  MPI_Comm newcomm;
  (void)MPI_Graph_create(mpi_comm, nnodes, (int*)index, 
		       (int*)edges, (int)reorder, &newcomm);
  return newcomm;
}
/* process-header: end of intracomm_inln.h */
/* process-header: including topology_inln.h */

//
//   ========   Cartcomm member functions  ========
//

inline
_REAL_MPI_::Cartcomm::Cartcomm(const MPI_Comm& data) {
  int status;
  if (MPI::Is_initialized() && (data != MPI_COMM_NULL)) {
    (void)MPI_Topo_test(data, &status) ;
    if (status == MPI_CART)
      mpi_comm = data;
    else
      mpi_comm = MPI_COMM_NULL;
  }
  else {
    mpi_comm = data;
  }
}

//
// Groups, Contexts, and Communicators
//

inline _REAL_MPI_::Cartcomm
_REAL_MPI_::Cartcomm::Dup() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  return newcomm;
}

//
//  Process Topologies
//

inline int
_REAL_MPI_::Cartcomm::Get_dim() const 
{
  int ndims;
  (void)MPI_Cartdim_get(mpi_comm, &ndims);
  return ndims;
}

inline void
_REAL_MPI_::Cartcomm::Get_topo(int maxdims, int dims[], MPI2CPP_BOOL_T periods[],
			       int coords[]) const
{
  int *int_periods = new int [maxdims];
  int i;
  for (i=0; i<maxdims; i++) {
    int_periods[i] = (int)periods[i];
  }
  (void)MPI_Cart_get(mpi_comm, maxdims, dims, int_periods, coords);
  for (i=0; i<maxdims; i++) {
    periods[i] = (MPI2CPP_BOOL_T)int_periods[i];
  }
  delete [] int_periods;
}

inline int
_REAL_MPI_::Cartcomm::Get_cart_rank(const int coords[]) const 
{
  int rank;
  (void)MPI_Cart_rank(mpi_comm, (int*)coords, &rank);
  return rank;
}
  
inline void
_REAL_MPI_::Cartcomm::Get_coords(int rank, int maxdims, int coords[]) const 
{
  (void)MPI_Cart_coords(mpi_comm, rank, maxdims, coords);
} 

inline void
_REAL_MPI_::Cartcomm::Shift(int direction, int disp,
			    int &rank_source, int &rank_dest) const 
{
  (void)MPI_Cart_shift(mpi_comm, direction, disp, &rank_source, &rank_dest);
}

inline _REAL_MPI_::Cartcomm
_REAL_MPI_::Cartcomm::Sub(const MPI2CPP_BOOL_T remain_dims[]) 
{
  int ndims;
  MPI_Cartdim_get(mpi_comm, &ndims);
  int* int_remain_dims = new int[ndims];
  for (int i=0; i<ndims; i++) {
    int_remain_dims[i] = (int)remain_dims[i];
  }
  MPI_Comm newcomm;
  (void)MPI_Cart_sub(mpi_comm, int_remain_dims, &newcomm);
  delete [] int_remain_dims;
  return newcomm;
}

inline int
_REAL_MPI_::Cartcomm::Map(int ndims, const int dims[], const MPI2CPP_BOOL_T periods[]) const 
{
  int *int_periods = new int [ndims];
  for (int i=0; i<ndims; i++) {
    int_periods[i] = (int) periods[i];
  }
  int newrank;
  (void)MPI_Cart_map(mpi_comm, ndims, (int*)dims, int_periods, &newrank);
  delete [] int_periods;
  return newrank;
}


#if VIRTUAL_FUNC_RET
inline _REAL_MPI_::Cartcomm&
_REAL_MPI_::Cartcomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Cartcomm* dup = new _REAL_MPI_::Cartcomm(newcomm);
  return *dup;
}
#else
inline _REAL_MPI_::Comm&
_REAL_MPI_::Cartcomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Cartcomm* dup = new _REAL_MPI_::Cartcomm(newcomm);
  return *dup;
}
#endif

//
//   ========   Graphcomm member functions  ========
//

inline
_REAL_MPI_::Graphcomm::Graphcomm(const MPI_Comm& data) {
  int status;
  if (MPI::Is_initialized() && (data != MPI_COMM_NULL)) {
    (void)MPI_Topo_test(data, &status) ;
    if (status == MPI_GRAPH)
      mpi_comm = data;
    else
      mpi_comm = MPI_COMM_NULL;
  }
  else {
    mpi_comm = data;
  }
}

//
// Groups, Contexts, and Communicators
//

inline _REAL_MPI_::Graphcomm
_REAL_MPI_::Graphcomm::Dup() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  return newcomm;
}

//
//  Process Topologies
//

inline void
_REAL_MPI_::Graphcomm::Get_dims(int nnodes[], int nedges[]) const 
{
  (void)MPI_Graphdims_get(mpi_comm, nnodes, nedges);
}

inline void
_REAL_MPI_::Graphcomm::Get_topo(int maxindex, int maxedges, int index[], 
	 int edges[]) const
{
  (void)MPI_Graph_get(mpi_comm, maxindex, maxedges, index, edges);
}

inline int
_REAL_MPI_::Graphcomm::Get_neighbors_count(int rank) const 
{
  int nneighbors;
  (void)MPI_Graph_neighbors_count(mpi_comm, rank, &nneighbors);
  return nneighbors;
}

inline void
_REAL_MPI_::Graphcomm::Get_neighbors(int rank, int maxneighbors, 
	      int neighbors[]) const 
{
  (void)MPI_Graph_neighbors(mpi_comm, rank, maxneighbors, neighbors);
}

inline int
_REAL_MPI_::Graphcomm::Map(int nnodes, const int index[], 
    const int edges[]) const 
{
  int newrank;
  (void)MPI_Graph_map(mpi_comm, nnodes, (int*)index, (int*)edges, &newrank);
  return newrank;
}

#if VIRTUAL_FUNC_RET
inline _REAL_MPI_::Graphcomm&
_REAL_MPI_::Graphcomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Graphcomm* dup = new _REAL_MPI_::Graphcomm(newcomm);
  return *dup;
}
#else
inline _REAL_MPI_::Comm&
_REAL_MPI_::Graphcomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Graphcomm* dup = new _REAL_MPI_::Graphcomm(newcomm);
  return *dup;
}
#endif
/* process-header: end of topology_inln.h */
/* process-header: including intercomm_inln.h */

inline _REAL_MPI_::Intercomm
_REAL_MPI_::Intercomm::Dup() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  return newcomm;
}

#if VIRTUAL_FUNC_RET
inline _REAL_MPI_::Intercomm&
_REAL_MPI_::Intercomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Intercomm* dup = new _REAL_MPI_::Intercomm(newcomm);
  return *dup;
}
#else
inline _REAL_MPI_::Comm&
_REAL_MPI_::Intercomm::Clone() const
{
  MPI_Comm newcomm;
  (void)MPI_Comm_dup(mpi_comm, &newcomm);
  _REAL_MPI_::Intercomm* dup = new _REAL_MPI_::Intercomm(newcomm);
  return *dup;
}
#endif

inline int
_REAL_MPI_::Intercomm::Get_remote_size() const 
{
  int size;
  (void)MPI_Comm_remote_size(mpi_comm, &size);
  return size;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Intercomm::Get_remote_group() const 
{
  MPI_Group group;
  (void)MPI_Comm_remote_group(mpi_comm, &group);
  return group;
}

inline _REAL_MPI_::Intracomm
_REAL_MPI_::Intercomm::Merge(MPI2CPP_BOOL_T high) 
{
  MPI_Comm newcomm;
  (void)MPI_Intercomm_merge(mpi_comm, (int)high, &newcomm);
  return newcomm;
}

/* process-header: end of intercomm_inln.h */
/* process-header: including group_inln.h */

//
// Groups, Contexts, and Communicators
//

inline int
_REAL_MPI_::Group::Get_size() const
{
  int size;
  (void)MPI_Group_size(mpi_group, &size);
  return size;
}

inline int
_REAL_MPI_::Group::Get_rank() const 
{
  int rank;
  (void)MPI_Group_rank(mpi_group, &rank);
  return rank;
}

inline void
_REAL_MPI_::Group::Translate_ranks (const _REAL_MPI_::Group& group1, int n,
				    const int ranks1[], 
				    const _REAL_MPI_::Group& group2, int ranks2[])
{
  (void)MPI_Group_translate_ranks(group1, n, (int*)ranks1, group2, (int*)ranks2);
}

inline int
_REAL_MPI_::Group::Compare(const _REAL_MPI_::Group& group1, const _REAL_MPI_::Group& group2)
{
  int result;
  (void)MPI_Group_compare(group1, group2, &result);
  return result;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Group::Union(const _REAL_MPI_::Group &group1, const _REAL_MPI_::Group &group2)
{
  MPI_Group newgroup;
  (void)MPI_Group_union(group1, group2, &newgroup);
  return newgroup;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Group::Intersect(const _REAL_MPI_::Group &group1, const _REAL_MPI_::Group &group2)
{
  MPI_Group newgroup;
  (void)MPI_Group_intersection( group1,  group2, &newgroup);
  return newgroup;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Group::Difference(const _REAL_MPI_::Group &group1, const _REAL_MPI_::Group &group2)
{
  MPI_Group newgroup;  
  (void)MPI_Group_difference(group1, group2, &newgroup);
  return newgroup;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Group::Incl(int n, const int ranks[]) const
{
  MPI_Group newgroup;
  (void)MPI_Group_incl(mpi_group, n, (int*)ranks, &newgroup);
  return newgroup;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Group::Excl(int n, const int ranks[]) const
{
  MPI_Group newgroup;
  (void)MPI_Group_excl(mpi_group, n, (int*)ranks, &newgroup);
  return newgroup;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Group::Range_incl(int n, const int ranges[][3]) const
{
  MPI_Group newgroup;
  (void)MPI_Group_range_incl(mpi_group, n, (int(*)[3])ranges, &newgroup);
  return newgroup;
}

inline _REAL_MPI_::Group
_REAL_MPI_::Group::Range_excl(int n, const int ranges[][3]) const
{
  MPI_Group newgroup;
  (void)MPI_Group_range_excl(mpi_group, n, (int(*)[3])ranges, &newgroup);
  return newgroup;
}

inline void
_REAL_MPI_::Group::Free()
{
  (void)MPI_Group_free(&mpi_group);
}
/* process-header: end of group_inln.h */
/* process-header: including op_inln.h */

#if _MPIPP_PROFILING_

inline
MPI::Op::Op() { }
  
inline
MPI::Op::Op(const MPI::Op& o) : pmpi_op(o.pmpi_op) { }
  
inline
MPI::Op::Op(const MPI_Op& o) : pmpi_op(o) { }

inline
MPI::Op::~Op() { }

inline
MPI::Op& MPI::Op::operator=(const MPI::Op& op) {
  pmpi_op = op.pmpi_op; return *this;
}

// comparison
inline MPI2CPP_BOOL_T
MPI::Op::operator== (const MPI::Op &a) {
  return (MPI2CPP_BOOL_T)(pmpi_op == a);
}

inline MPI2CPP_BOOL_T
MPI::Op::operator!= (const MPI::Op &a) {
  return (MPI2CPP_BOOL_T)!(*this == a);
}

// inter-language operability
inline MPI::Op&
MPI::Op::operator= (const MPI_Op &i) { pmpi_op = i; return *this; }

inline
MPI::Op::operator MPI_Op () const { return pmpi_op; }

//inline
//MPI::Op::operator MPI_Op* () { return pmpi_op; }


#else  // ============= NO PROFILING ===================================

// construction
inline
MPI::Op::Op() : mpi_op(MPI_OP_NULL) { }

inline
MPI::Op::Op(const MPI_Op &i) : mpi_op(i) { }

inline
MPI::Op::Op(const MPI::Op& op)
  : op_user_function(op.op_user_function), mpi_op(op.mpi_op) { }

inline 
MPI::Op::~Op() 
{ 
#if _MPIPP_DEBUG_
  mpi_op = MPI_OP_NULL;
  op_user_function = 0;
#endif
}  

inline MPI::Op&
MPI::Op::operator=(const MPI::Op& op) {
  mpi_op = op.mpi_op;
  op_user_function = op.op_user_function;
  return *this;
}

// comparison
inline MPI2CPP_BOOL_T
MPI::Op::operator== (const MPI::Op &a) { return (MPI2CPP_BOOL_T)(mpi_op == a.mpi_op); }

inline MPI2CPP_BOOL_T
MPI::Op::operator!= (const MPI::Op &a) { return (MPI2CPP_BOOL_T)!(*this == a); }

// inter-language operability
inline MPI::Op&
MPI::Op::operator= (const MPI_Op &i) { mpi_op = i; return *this; }

inline
MPI::Op::operator MPI_Op () const { return mpi_op; }

//inline
//MPI::Op::operator MPI_Op* () { return &mpi_op; }

#endif



inline void
_REAL_MPI_::Op::Init(_REAL_MPI_::User_function *func, MPI2CPP_BOOL_T commute)
{
  (void)MPI_Op_create(op_intercept , (int) commute, &mpi_op);
  op_user_function = (User_function*)func;
}


inline void
_REAL_MPI_::Op::Free()
{
  (void)MPI_Op_free(&mpi_op);
}





/* process-header: end of op_inln.h */
/* process-header: including errhandler_inln.h */

#if _MPIPP_PROFILING_

inline PMPI::Errhandler::Errhandler(const PMPI::Errhandler& e)
  : mpi_errhandler(e.mpi_errhandler), handler_fn(e.handler_fn) { }

inline PMPI::Errhandler&
PMPI::Errhandler::operator=(const PMPI::Errhandler& e)
{
  handler_fn = e.handler_fn;
  mpi_errhandler = e.mpi_errhandler;
  return *this;
}

inline MPI2CPP_BOOL_T
PMPI::Errhandler::operator==(const PMPI::Errhandler &a)
{
  return (MPI2CPP_BOOL_T)(mpi_errhandler == a.mpi_errhandler);
}

#endif

inline void
_REAL_MPI_::Errhandler::Free()
{
  (void)MPI_Errhandler_free(&mpi_errhandler);
}




/* process-header: end of errhandler_inln.h */
/* process-header: including status_inln.h */

//
// Point-to-Point Communication
//

inline int
_REAL_MPI_::Status::Get_count(const _REAL_MPI_::Datatype& datatype) const
{
  int count;
  //(MPI_Status*) is to cast away the const
  (void)MPI_Get_count((MPI_Status*)&mpi_status, datatype, &count);
  return count;
}

inline MPI2CPP_BOOL_T
_REAL_MPI_::Status::Is_cancelled() const
{
  int t;
  (void)MPI_Test_cancelled((MPI_Status*)&mpi_status, &t);
  return (MPI2CPP_BOOL_T) t;
}

inline int
_REAL_MPI_::Status::Get_elements(const _REAL_MPI_::Datatype& datatype) const
{
  int count;
  (void)MPI_Get_elements((MPI_Status*)&mpi_status, datatype, &count);
  return count;
}

//
// Status Access
//
inline int
_REAL_MPI_::Status::Get_source() const
{
  int source;
  source = mpi_status.MPI_SOURCE;
  return source;
}

inline void
_REAL_MPI_::Status::Set_source(int source)
{
  mpi_status.MPI_SOURCE = source;
}

inline int
_REAL_MPI_::Status::Get_tag() const
{
  int tag;
  tag = mpi_status.MPI_TAG;
  return tag;
}

inline void
_REAL_MPI_::Status::Set_tag(int tag)
{
  mpi_status.MPI_TAG = tag;
}

inline int
_REAL_MPI_::Status::Get_error() const
{
  int error;
  error = mpi_status.MPI_ERROR;
  return error;
}

inline void
_REAL_MPI_::Status::Set_error(int error)
{
  mpi_status.MPI_ERROR = error;
}
/* process-header: end of status_inln.h */

#endif


