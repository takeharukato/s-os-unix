/*
   utility functions
   $Id: util.c,v 1.2 1996/10/24 17:54:45 tate Exp $

   tate@spa.is.uec.ac.jp
*/
#include "simz80.h"

void
z80_push(WORD x){
    PutBYTE(--sp, (x) >> 8);
    PutBYTE(--sp, x);
}

WORD
z80_pop(void){
    WORD	r;

    /* poor code, against some compiler error */
    r = (WORD) GetBYTE(sp++);
    r += ((WORD) GetBYTE(sp++) << 8);
    return(r);
}

