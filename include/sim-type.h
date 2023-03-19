/** z80 emulator type definition
 */
#ifndef	_SIM_TYPE_H
#define	_SIM_TYPE_H

#include <limits.h>

#if UCHAR_MAX == 255
typedef unsigned char	BYTE;
#else
#error Need to find an 8-bit type for BYTE
#endif

#if USHRT_MAX == 65535
typedef unsigned short	WORD;
#else
#error Need to find an 16-bit type for WORD
#endif

/* FASTREG needs to be at least 16 bits wide and efficient for the
   host architecture */
#if UINT_MAX >= 65535
typedef unsigned int	FASTREG;
#else
typedef unsigned long	FASTREG;
#endif

/* FASTWORK needs to be wider than 16 bits and efficient for the host
   architecture */
#if UINT_MAX > 65535
typedef unsigned int	FASTWORK;
#else
typedef unsigned long	FASTWORK;
#endif

#endif  /*  _SIM_TYPE_H  */
