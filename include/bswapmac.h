/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator byte swap macros                                   */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_BSWAPMAC_H)
#define _BSWAPMAC_H

#include "config.h"
#include "sim-type.h"
#include "misc.h"

#if defined(HAVE_BYTESWAP_H)
#include <byteswap.h>
#else
#define bswap_16(_x)							\
	((WORD)( ( ( (_x) >> 8 ) & 0xff ) | ( ( (_x) & 0xff ) << 8 ) ))
#endif  /*  HAVE_BYTESWAP_H  */
#define bswap_word_z80_to_host(_x) ( is_little_endian() ? (_x) : bswap_16(_x) )
#define bswap_word_host_to_z80(_x) bswap_word_z80_to_host((_x))


#endif  /*  _BSWAPMAC_H  */
