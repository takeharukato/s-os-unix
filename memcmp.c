/*
   This module is in public domain.

   tate@spa.is.uec.ac.jp
   $Id: memcmp.c,v 1.1 1999/02/19 16:48:32 tate Exp $
*/
#include <stdlib.h>

int
memcmp(unsigned char *b1, unsigned char *b2, size_t len){
    register int	r;
    register unsigned char	c;
    unsigned char	*p1, *p2;

    r = 0;
    while((--len > 0) &&
	(r = (c = (*p1++ - *b2++)) == 0) &&
	c != '\0')
	;
    return(r);
}
