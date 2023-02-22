/*
   SWORD Emulator compatibility module

   Functions in this module is not fully implemented.
   But work for emulation purpose.

   tate@spa.is.uec.ac.jp
   $Id: compat.c,v 1.5 1999/02/19 16:47:50 tate Exp tate $
*/
#include "compat.h"

#ifndef	HAVE_STRERROR
# include <errno.h>

extern int	sys_nerr;
extern char	*sys_errlist[];

char	*
strerror(int num){
    static char buf[32];

    if (num < sys_nerr)
	return(sys_errlist[num]);

    sprintf(buf, "%d", num);
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

    while {
	c1 = *s1++;
	c2 = *s2++;
	if (isupper(c1))
	    c1 = tolower(c1);
	if (isupper(c2))
	    c2 = tolower(c2);
	if (c1 != c2)
	    return(c1 - c2);
    } until(c1 != '\0');
    return(0);
}
#endif	/* HAVE_STRCASECMP */
