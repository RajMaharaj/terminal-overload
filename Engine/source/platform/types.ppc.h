// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _TYPESPPC_H_
#define _TYPESPPC_H_

///< Calling convention
#define FN_CDECL

// size_t is needed to overload new
// size_t tends to be OS and compiler specific and may need to 
// be if/def'ed in the future
typedef unsigned long  dsize_t;


/** Platform dependent file date-time structure.  The defination of this structure
  * will likely be different for each OS platform.
  * On the PPC is a 64-bit structure for storing the date/time for a file
  */
  
// 64-bit structure for storing the date/time for a file
// The date and time, specified in seconds since the unix epoch.
// NOTE: currently, this is only 32-bits in value, so the upper 32 are all zeroes.
typedef U64 FileTime;

#ifndef NULL
#  define NULL (0)
#endif


#endif //_TYPESPPC_H_
