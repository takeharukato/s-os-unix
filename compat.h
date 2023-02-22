/*
   SWORD Emulator compatibility module

   tate@spa.is.uec.ac.jp
   $Id: compat.h,v 1.4 1999/02/19 16:48:08 tate Exp tate $
*/

#ifndef	_COMPAT_H_
#define	_COMPAT_H_

/* return value of putchar routine called from tputs */
#ifndef TYPE_TPUTS
# define TYPE_TPUTS	int
#endif

#ifndef	HAVE_STRERROR
char	*strerror(int num);
#endif

#ifndef	HAVE_MEMMOVE
# define memmove(dst,src,len) bcopy(src,dst,len)
#endif

#ifdef	HAVE_FCNTL
# include <fcntl.h>
# ifndef O_NONBLOCK
#  define O_NONBLOCK	FNDELAY
# endif
#endif

#ifndef	HAVE_SIGPROCMASK	/* aka NO_POSIX_SIGNAL */
# define SIG_BLOCK	1
# define SIG_UNBLOCK	2
# define SIG_SETMASK	3
typedef int	sigset_t;
struct sigaction {
	int	(*sa_handler)();
        int     sa_mask;
        int     sa_flags;
};

int	sigprocmask(int how, sigset_t *set, sigset_t *oset);
int	sigemptyset(sigset_t *set);
int	sigaddset(sigset_t *set, int signo);
# define sigaction(x,y,z)	sigvec(x,(struct sigvec *)y,(struct sigvec *)z)
#endif	/* HAVE_SIGPROCMASK (aka NO_POSIX_SIGNAL) */

#ifdef	HAVE_TERMIOS
# ifndef HAVE_CFMAKERAW
#  include <termios.h>
void	cfmakeraw(struct termios *t);
# endif	/* !HAVE_CFMAKERAW */
#endif	/* HAVE_TERMIOS */

#ifndef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *2);
#endif

#endif
