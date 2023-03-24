/*
 * SWORD Emulator
 * Miscellaneous functions
 */

#ifndef	_MISC_H_
#define	_MISC_H_

#include "config.h"
#include <stdio.h>
#include <stdlib.h>

#define FALSE    (!!0)
#define TRUE     ( !FALSE )

/** assert for S-OS on UNIX
   @param[in] cond the condition must be TRUE.
 */
#define sos_assert(cond) do {						\
	if ( !(cond) ) {                                                \
		fprintf(stderr, "Assertion : [file:%s func %s line:%d ]\n", \
		    __FILE__, __func__, __LINE__);			\
		abort();						\
	}								\
	}while(0)

/** assert when the program comes to the point which must not be reached.
 */
#define sos_assert_no_reach() do {						\
		fprintf(stderr, "No reach assertion : [file:%s func %s line:%d ]\n", \
		    __FILE__, __func__, __LINE__);			\
		abort();						\
	}while(0)


int check_file_exists(const char *_path, int _flags);
int ascii_to_int(const char *_numstr, int *_vp);
#endif  /*  _MISC_H_  */
