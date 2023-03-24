/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator freestanding header inclusion                      */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_FREESTANDING_H)
#define  _FREESTANDING_H

#include "config.h"

#if defined(HAVE_FLOAT_H)
#include <float.h>
#endif  /*  HAVE_FLOAT_H  */
#if defined(HAVE_ISO646_H)
#include <iso646.h>
#endif   /*  HAVE_ISO646_H  */
#if defined(HAVE_LIMITS_H)
#include <limits.h>
#endif  /*  HAVE_LIMITS_H  */
#if defined(HAVE_STDARG_H)
#include <stdarg.h>
#endif  /*  HAVE_STDARG_H  */
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#endif  /*  HAVE_STDBOOL_H  */
#if defined(HAVE_STDDEF_H)
#include <stddef.h>
#endif  /*  HAVE_STDDEF_H  */
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif  /*  HAVE_STDINT_H  */
#endif  /*  !_FREESTANDING_H  */
