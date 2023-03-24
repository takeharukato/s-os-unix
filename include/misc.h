/*
 * SWORD Emulator
 * Miscellaneous functions
 */

#ifndef	_MISC_H_
#define	_MISC_H_

#include "config.h"
#include "freestanding.h"

#include <stdio.h>
#include <stdlib.h>

#include "sim-type.h"

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

/** Calc aligned address
    @param[in] _val   The value to align
    @param[in] _align Alignment
 */
#define SOS_CALC_ALIGN(_val, _align)		\
	( (WORD)(	( (WORD)(_val) & ~( ( (WORD)(_align) ) - 1 ) ) \
	    & ( (WORD)(~((WORD)0)) ) ) )

/** Calc next aligned address
    @param[in] _val   The value to align
    @param[in] _align Alignment
 */
#define CALC_NEXT_ALIGN(_val, _align)		\
	SOS_CALC_ALIGN(( (_val) + ( (_align) - 1 ) ), _align)
int check_file_exists(const char *_path, int _flags);
int ascii_to_int(const char *_numstr, int *_vp);
const char *refer_file_extention(const char *_fname);
#endif  /*  _MISC_H_  */
