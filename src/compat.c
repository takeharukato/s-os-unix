/*
   SWORD Emulator compatibility module

   Functions in this module is not fully implemented.
   But work for emulation purpose.

   tate@spa.is.uec.ac.jp
   $Id: compat.c,v 1.5 1999/02/19 16:47:50 tate Exp tate $
*/

#include "config.h"
#include "compat.h"
#include <ctype.h>
#ifndef	HAVE_STRERROR
# include <errno.h>

extern int	sys_nerr;
extern char	*sys_errlist[];

char	*
strerror(int num){
    static char buf[32];

    if (num < sys_nerr)
	return(sys_errlist[num]);

    snprintf(buf, 32, "%d", num);
    return(buf);
}
#endif	/* !HAVE_STRERROR */


#ifndef	HAVE_SIGPROCMASK
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

int
sigprocmask(int how, sigset_t *set, sigset_t *oset){
    sigset_t	dummy;

    if (oset == NULL)
	oset = &dummy;
    switch(how){
      case SIG_BLOCK:
	*oset = sigblock(*set);
	return(0);
      case SIG_UNBLOCK:
	*oset = sigblock(sigblock(0) & ~(*set));
	return(0);
      case SIG_SETMASK:
	*oset = sigsetmask(*set);
	return(0);
    }
    errno = EINVAL;
    return(1);
}

int
sigemptyset(sigset_t *set){
    *set = 0;
    return(0);
}

int
sigaddset(sigset_t *set, int signo){
    *set |= sigmask(signo);
    return(0);
}
#endif	/* !HAVE_SIGPROCMASK (aka NO_POSIX_SIGNAL) */

#ifdef	HAVE_TERMIOS_H
# ifndef HAVE_CFMAKERAW
#include <termios.h>
void
cfmakeraw(struct termios *term_ios){
    /* note: cfmakeraw() is BSD extention, this alternative code will work
	 on most systems but not perfect */
    term_ios->c_iflag &= ~(IGNBRK|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF);
#ifdef	IMAXBEL
    term_ios->c_iflag &= ~(IMAXBEL);
#endif
    term_ios->c_oflag &= ~(OPOST);
    term_ios->c_cflag &= ~(CSIZE|PARENB);
    term_ios->c_cflag |= ~(CS8);
    term_ios->c_lflag &= ~(ECHO|ECHONL|ICANON);
}
# endif	/* !HAVE_CFMAKERAW */
#endif	/* HAVE_TERMIOS */

#ifndef HAVE_STRCASECMP
int
strcasecmp(const char *s1, const char *2){
    unsigned char c1, c2;

    do{
	c1 = *s1++;
	c2 = *s2++;
	if (isupper(c1))
	    c1 = tolower(c1);
	if (isupper(c2))
	    c2 = tolower(c2);
	if (c1 != c2)
	    return(c1 - c2);
    } while(c1 != '\0');
    return(0);
}
#endif	/* !HAVE_STRCASECMP */

#ifndef HAVE_STRRCHR
char *
strrchr(const char *s, int c){
	ssize_t len;

	if ( s == NULL )
		return NULL;


	for(len = strlen(s); len >= 0; --len)
		if ( (int)s[len] == c )
			return &s[len];  /* found */

	return NULL;  /* not found */
}
#endif	/* !HAVE_STRRCHR */

#ifndef HAVE_MEMCMP
int
memcmp(const void *s1, const void *s2, size_t n){
	size_t i;

	for( i = 0; n > i; ++i )
		if ( s1[i] < s2[i] )
			return -1;
		else if ( s1[i] > s2[i] )
			return 1;

	return 0;
}
#endif  /* !HAVE_MEMCMP */

#ifndef HAVE_STRTOL
/** convert a string to a long integer
    @remark this implementation is derived from FreeBSD.
    @param[in]  nptr   The string to be converted.
    @param[in]  endptr The pointer to points the first non-converted character.
    @param[in]  base   The number to convert the strings according to.
    @return the converted value from nptr. ENDPTR points the first
    non-converted character if an error occurred.
 */
long int
strtol(const char *nptr, char **endptr, int base){
	const char *s;
	unsigned long acc;
	char c;
	unsigned long cutoff;
	int neg, any, cutlim;

	s = nptr;
	do {
		c = *s++; /* skip space */
	}while( isspace((int)c) );

	/*
	 * handle sign character
	 */
	if ( c == '-' ) {

		neg = 1;
		c = *s++;

	} else {

		neg = 0;
		if (c == '+')
			c = *s++;
	}

	acc = any = 0;  /* initialize converted value */

	/*
	 * determine the base
	 */
	if ( ( ( base == 0 ) || ( base == 16 ) ) &&
	    ( c == '0' ) && ( ( *s == 'x' ) || ( *s == 'X' ) ) &&
	    ( isdigit((int)s[1]) ||isxdigit((int)s[1]) ) ) {

		/*
		 * Hex
		 */
		c = s[1];
		s += 2;
		base = 16;
	} else if ( base == 0 )
		if ( c == '0' )
			base = 8;  /* octal */
		else
			base = 10; /* decimal */

	if ( ( base < 2 ) || ( base > 36 ) )
		goto noconv;

	/* Cutoff and cutlim are used for the reason as the follows
	 * (The sentences are quoted from the comment in strtol_l
	 * function in FreeBSD).:
	 *
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 */
	cutoff = neg ? (unsigned long)(-(LONG_MIN + LONG_MAX)) + LONG_MAX
	    : LONG_MAX;
	cutlim = cutoff % base;
	cutoff /= base;

	for ( ; ; c = *s++) {

		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'A' && c <= 'Z')
			c -= 'A' - 10;
		else if (c >= 'a' && c <= 'z')
			c -= 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = neg ? LONG_MIN : LONG_MAX;
		errno = ERANGE;
	} else if (!any) {
noconv:
		errno = EINVAL;
	} else if (neg)
		acc = -acc;
	if (endptr != NULL)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}
#endif  /*  !HAVE_STRTOL  */
