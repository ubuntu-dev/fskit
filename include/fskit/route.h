/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License version 3 or later as
   published by the Free Software Foundation. For the terms of this
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU Lesser General
   Public License, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU Lesser General Public License for more details.

   Alternatively, you are free to use this program under the terms of the
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#ifndef _FSKIT_ROUTE_H_
#define _FSKIT_ROUTE_H_

#include <regex.h>

#include "common.h"
#include "fskit.h"

// prototypes
struct fskit_core;
struct fskit_dir_entry;

// route match methods
#define FSKIT_ROUTE_MATCH_CREATE                0
#define FSKIT_ROUTE_MATCH_MKDIR                 1
#define FSKIT_ROUTE_MATCH_MKNOD                 2
#define FSKIT_ROUTE_MATCH_OPEN                  3
#define FSKIT_ROUTE_MATCH_READDIR               4
#define FSKIT_ROUTE_MATCH_READ                  5
#define FSKIT_ROUTE_MATCH_WRITE                 6
#define FSKIT_ROUTE_MATCH_TRUNC                 7
#define FSKIT_ROUTE_MATCH_CLOSE                 8
#define FSKIT_ROUTE_MATCH_DETACH                9
#define FSKIT_ROUTE_MATCH_STAT                  10
#define FSKIT_ROUTE_MATCH_SYNC                  11
#define FSKIT_ROUTE_MATCH_RENAME                12
#define FSKIT_ROUTE_NUM_ROUTE_TYPES             13

// route consistency disciplines
#define FSKIT_SEQUENTIAL        1       // route method calls will be serialized
#define FSKIT_CONCURRENT        2       // route method calls will be concurrent
#define FSKIT_INODE_SEQUENTIAL  3       // route method calls on the same inode will be serialized

// common routes
#define FSKIT_ROUTE_ANY         "/([^/]+[/]*)*"


// metadata about the patch matched to the route
struct fskit_route_metadata {
   char* path;                  // the path matched
   int argc;                    // number of matches
   char** argv;                 // each matched string in the path regex
   
   struct fskit_entry* parent;  // parent entry (creat(), mknod(), mkdir(), rename() only)
   
   struct fskit_entry* new_parent;      // parent entry of the destination (rename() only)
   char* new_path;                      // path to rename to (rename() only)
};

// for backwards compatibility
#define fskit_match_group fskit_route_metadata

// method callback signatures to match on path route
typedef int (*fskit_entry_route_create_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, mode_t, void**, void** );
typedef int (*fskit_entry_route_mknod_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, mode_t, dev_t, void** );
typedef int (*fskit_entry_route_mkdir_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, mode_t, void** );
typedef int (*fskit_entry_route_open_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, int, void** );         // open() and opendir()
typedef int (*fskit_entry_route_close_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, void* );              // close() and closedir()
typedef int (*fskit_entry_route_io_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, char*, size_t, off_t, void* );  // read() and write()
typedef int (*fskit_entry_route_trunc_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, off_t, void* );
typedef int (*fskit_entry_route_sync_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry* );         // fsync(), fdatasync()
typedef int (*fskit_entry_route_stat_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, struct stat* );
typedef int (*fskit_entry_route_readdir_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, struct fskit_dir_entry**, size_t );
typedef int (*fskit_entry_route_detach_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, void* );             // unlink() and rmdir()
typedef int (*fskit_entry_route_rename_callback_t)( struct fskit_core*, struct fskit_route_metadata*, struct fskit_entry*, char const*, struct fskit_entry* );

// which method will be called?
union fskit_route_method {
   fskit_entry_route_create_callback_t       create_cb;
   fskit_entry_route_mknod_callback_t        mknod_cb;
   fskit_entry_route_mkdir_callback_t        mkdir_cb;
   fskit_entry_route_open_callback_t         open_cb;
   fskit_entry_route_close_callback_t        close_cb;
   fskit_entry_route_io_callback_t           io_cb;
   fskit_entry_route_trunc_callback_t        trunc_cb;
   fskit_entry_route_sync_callback_t         sync_cb;
   fskit_entry_route_stat_callback_t         stat_cb;
   fskit_entry_route_readdir_callback_t      readdir_cb;
   fskit_entry_route_detach_callback_t       detach_cb;
   fskit_entry_route_rename_callback_t       rename_cb;
};

// a path route
struct fskit_path_route {

   char* path_regex_str;                // string-ified regex
   int num_expected_matches;            // number of expected match groups (upper bound)
   regex_t path_regex;                  // compiled regular expression

   int consistency_discipline;          // concurrent or sequential call?

   int route_type;                      // one of FSKIT_ROUTE_MATCH_*
   fskit_route_method method;           // which method to call

   pthread_rwlock_t lock;               // lock used to enforce the consistency discipline
};


// I/O continuation for successful read/write/trunc (i.e. to be called with the route's consistency discipline enforced)
typedef int (*fskit_route_io_continuation)( struct fskit_core*, struct fskit_entry*, off_t, ssize_t );

// dispatch arguments
struct fskit_route_dispatch_args {

   int flags;           // open() only

   mode_t mode;         // create(), mknod() only
   dev_t dev;           // mknod() only

   void* inode_data;    // create(), mkdir(), unlink(), rmdir() only.  In create() and mkdir(), this is an output value.
   void* handle_data;   // create(), open(), opendir(), close() only.  In open() and opendir(), this is an output value.

   char* iobuf;         // read(), write() only.  In read(), this is an output value.
   size_t iolen;        // read(), write() only
   off_t iooff;         // read(), write(), trunc() only
   fskit_route_io_continuation io_cont;  // read(), write(), trunc() only

   struct fskit_dir_entry** dents;        // readdir() only
   uint64_t num_dents;

   struct stat* sb;      // stat() only
   
   struct fskit_entry* parent;  // create(), mkdir(), mknod(), rename() (guaranteed to be write-locked)
   
   struct fskit_entry* new_parent;      // rename() only (guaranteed to be write-locked)
   struct fskit_entry* dest;    // rename() only (not locked)
   char const* new_path;      // rename() only
};

// private API...

// populate route dispatch arguments (internal API)
int fskit_route_create_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, mode_t mode );
int fskit_route_mknod_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, mode_t mode, dev_t dev );
int fskit_route_mkdir_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* parent, mode_t mode );
int fskit_route_open_args( struct fskit_route_dispatch_args* dargs, int flags );
int fskit_route_close_args( struct fskit_route_dispatch_args* dargs, void* handle_data );
int fskit_route_readdir_args( struct fskit_route_dispatch_args* dargs, struct fskit_dir_entry** dents, uint64_t num_dents );
int fskit_route_io_args( struct fskit_route_dispatch_args* dargs, char* iobuf, size_t iolen, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont );
int fskit_route_trunc_args( struct fskit_route_dispatch_args* dargs, off_t iooff, void* handle_data, fskit_route_io_continuation io_cont );
int fskit_route_detach_args( struct fskit_route_dispatch_args* dargs, void* inode_data );
int fskit_route_stat_args( struct fskit_route_dispatch_args* dargs, struct stat* sb );
int fskit_route_sync_args( struct fskit_route_dispatch_args* dargs );
int fskit_route_rename_args( struct fskit_route_dispatch_args* dargs, struct fskit_entry* old_parent, char const* new_path, struct fskit_entry* new_parent, struct fskit_entry* dest );

// call user-supplied routes (internal API)
int fskit_route_call_create( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_mknod( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_mkdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_open( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_close( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_readdir( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_read( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_write( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_detach( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_stat( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_sync( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );
int fskit_route_call_rename( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_route_dispatch_args* dargs, int* cbrc );

// memory management (internal API)
int fskit_path_route_free( struct fskit_path_route* route );

// public API...
extern "C" {

// define various types of routes
int fskit_route_create( struct fskit_core* core, char const* route_regex, fskit_entry_route_create_callback_t create_cb, int consistency_discipline );
int fskit_route_mknod( struct fskit_core* core, char const* route_regex, fskit_entry_route_mknod_callback_t create_cb, int consistency_discipline );
int fskit_route_mkdir( struct fskit_core* core, char const* route_regex, fskit_entry_route_mkdir_callback_t mkdir_cb, int consistency_discipline );
int fskit_route_open( struct fskit_core* core, char const* route_regex, fskit_entry_route_open_callback_t open_cb, int consistency_discipline );
int fskit_route_close( struct fskit_core* core, char const* route_regex, fskit_entry_route_close_callback_t close_cb, int consistency_discipline );
int fskit_route_readdir( struct fskit_core* core, char const* route_regex, fskit_entry_route_readdir_callback_t readdir_cb, int consistency_discipline );
int fskit_route_read( struct fskit_core* core, char const* route_regex, fskit_entry_route_io_callback_t io_cb, int consistency_discipline );
int fskit_route_write( struct fskit_core* core, char const* route_regex, fskit_entry_route_io_callback_t io_cb, int consistency_discipline );
int fskit_route_trunc( struct fskit_core* core, char const* route_regex, fskit_entry_route_trunc_callback_t io_cb, int consistency_discipline );
int fskit_route_detach( struct fskit_core* core, char const* route_regex, fskit_entry_route_detach_callback_t detach_cb, int consistency_discipline );
int fskit_route_stat( struct fskit_core* core, char const* route_regex, fskit_entry_route_stat_callback_t stat_cb, int consistency_discipline );
int fskit_route_sync( struct fskit_core* core, char const* route_regex, fskit_entry_route_sync_callback_t sync_cb, int consistency_discipline );
int fskit_route_rename( struct fskit_core* core, char const* route_regex, fskit_entry_route_rename_callback_t rename_cb, int consistency_discipline );

// undefine various types of routes
int fskit_unroute_create( struct fskit_core* core, int route_handle );
int fskit_unroute_mknod( struct fskit_core* core, int route_handle );
int fskit_unroute_mkdir( struct fskit_core* core, int route_handle );
int fskit_unroute_open( struct fskit_core* core, int route_handle );
int fskit_unroute_close( struct fskit_core* core, int route_handle );
int fskit_unroute_readdir( struct fskit_core* core, int route_handle );
int fskit_unroute_read( struct fskit_core* core, int route_handle );
int fskit_unroute_write( struct fskit_core* core, int route_handle );
int fskit_unroute_trunc( struct fskit_core* core, int route_handle );
int fskit_unroute_detach( struct fskit_core* core, int route_handle );
int fskit_unroute_stat( struct fskit_core* core, int route_handle );
int fskit_unroute_sync( struct fskit_core* core, int route_handle );
int fskit_unroute_rename( struct fskit_core* core, int route_handle );

// unroute everything 
int fskit_unroute_all( struct fskit_core* core );

// access route metadata 
char* fskit_route_metadata_path( struct fskit_route_metadata* route_metadata );
int fskit_route_metadata_num_match_groups( struct fskit_route_metadata* route_metadata );
char** fskit_route_metadata_match_groups( struct fskit_route_metadata* route_metadata );
struct fskit_entry* fskit_route_metadata_parent( struct fskit_route_metadata* route_metadata );
char* fskit_route_metadata_new_path( struct fskit_route_metadata* route_metadata );
struct fskit_entry* fskit_route_metadata_new_parent( struct fskit_route_metadata* route_metadata );

}

#endif
