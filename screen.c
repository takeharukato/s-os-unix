/*
   SWORD Emulator screen control module
   $Id: screen.c,v 1.9 1999/02/19 16:49:01 tate Exp tate $

   tate@spa.is.uec.ac.jp
*/
#include <stdio.h>
#include <stdlib.h>
#ifdef	HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifdef	HAVE_TERMIOS_H
# include <termios.h>
#ifdef	HAVE_TERM_H
# include <term.h>
#endif
#else
# include <sgtty.h>
# include <sys/file.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#ifdef HAVE_CURSES_H
# include <curses.h>		/* need only termcap facility, however major */
#else
# ifdef HAVE_TERMCAP_H
#  include <termcap.h>
# else
#  error "Both curses.h and termcap.h are not available."
# endif
#endif
#ifdef	OPT_DELAY_FLUSH
# include <sys/time.h>
#endif
#include "compat.h"
#include "sos.h"
#include "screen.h"

#ifndef SCR_MAXLINES
# define SCR_MAXLINES	(25)	/* maximam lines of virtual screen */
#endif
#ifndef	SCR_MAXWIDTH
# define SCR_MAXWIDTH	(80)	/* maximam width of virtual screen */
#endif

#ifdef	OPT_DELAY_FLUSH
# ifndef OPT_DELAY_FLUSH_TIME	/* interval time of flush in usec */
#  define OPT_DELAY_FLUSH_TIME	20	/* 1/60 sec */
# endif
#endif

#define	SCR_TABLEN	(8)	/* length of tab */

#define	SCREEN		(1)		/* stdout */
/* flags */
#define	SCR_F_NONE	(0)
#ifndef	OPT_DELAY_FLUSH
# define SCR_F_IMM	(1)
#else
# define SCR_F_IMM	(0)
#endif
/* attribute */
#define	SCR_A_CLEAN	(' ')
#define	SCR_A_DIRTY	('D')
/* line attribute */
#define	SCR_LA_NONE	(0)
#define	SCR_LA_DIRTY	(1)	/* this line is darty (not flushed) */
#define	SCR_LA_CONT	(2)	/* this line is contine to next line */

static unsigned char *scr_vchr[SCR_MAXLINES+1];	/* virtual screen */
static unsigned char *scr_vattr[SCR_MAXLINES+1];/* virtual screen attribute */
static unsigned char scr_vlattr[SCR_MAXLINES+1];/* virtual line attribute */
static int scr_vx, scr_vy;	/* cursor posision on virtual screen */
static int scr_px, scr_py;	/* cursor posision on physical screen */
static int scr_vw, scr_vh;	/* Width and Height of virtual screen */
static int scr_pw, scr_ph;	/* Width and Height of physical screen */

/* termcap entries */
static char *scr_tc_sf_str;	/* scroll commmand */
static char *scr_tc_cl_str;	/* clear screen & home cursor */
static char *scr_tc_ho_str;	/* home cursor */
static char *scr_tc_cm_str;	/* cursor move (y, x) */
static char *scr_tc_bl_str;	/* bell */
static char *scr_tc_vi_str;	/* make cursor visible */
static char *scr_tc_ve_str;	/* make cursor appear normal */

/* save for restore terminal informations */
#ifdef	HAVE_TERMIOS_H
 static struct termios	term_ios;
 static struct termios	term_ios_orig;
#else
 static struct sgttyb	term_sgtty;
 static struct sgttyb	term_sgtty_orig;
#endif
static int	out_flags;
static int	out_blocking = 1;	/* now, terminal is blocking mode */
static int	out_blocking_orig;

/* break key hack */
#define	SCR_BREAK	(0x1b)	/* break key code on S-OS */
static int	breaked = 0;

/* declaration of signal handler */
RETSIGTYPE	scr_stopr();
RETSIGTYPE	scr_intr();
#ifdef	OPT_DELAY_FLUSH
RETSIGTYPE	scr_alrm();
#endif

/* key input conversion */
static int	scr_capson = 0;

/* against signals in critical region */
#define	ON_CRITICAL	{ (void)sigprocmask(SIG_BLOCK, &scr_intset, NULL); }
#define	OFF_CRITICAL	{ (void)sigprocmask(SIG_UNBLOCK, &scr_intset, NULL); }
static sigset_t	scr_intset;
static int scr_in_signal = 0;	/* "in signal hander" flag */

/* cursor control status */
static int scr_cur_visible = 1;

/* cut & paste buffer */
static unsigned char	scr_cutbuf[SCR_MAXLINES * SCR_MAXWIDTH + 1];

/* screen modes */
static int scr_mode_insert = 1;	/* insert mode */

/* keyboard functions for screen edit */
void	scr_key_backspace(void), scr_key_delete(void),
	scr_key_top(void), scr_key_end(void),
	scr_key_up(void), scr_key_down(void),
	scr_key_forward(void), scr_key_back(void),
	scr_key_redraw(void), scr_key_kill(void), scr_key_tab(void),
	scr_key_yank(void), scr_key_imode(void), scr_key_clear(void);
struct keyfunc {
    char	*funcname;	/* name of function */
    void	(*func)(void);	/* pointer to function */
};
typedef struct keyfunc keyfunc_t;
static keyfunc_t keyfuncs[] = {
    {"backspace", scr_key_backspace},
    {"delete", scr_key_delete},
    {"begin", scr_key_top},
    {"end", scr_key_end},
    {"up", scr_key_up},
    {"down", scr_key_down},
    {"forward", scr_key_forward},
    {"back", scr_key_back},
    {"redraw", scr_key_redraw},
    {"kill", scr_key_kill},
    {"tab", scr_key_tab},
    {"yank", scr_key_yank},
    {"imode", scr_key_imode},
    {"clear", scr_key_clear},
    {NULL, NULL}
};
static int	keymap[(int)' '];

/* META functions for terminal mode control */
/* make raw+signal mode */
void
scr_term_makeraw(void){
#ifdef	HAVE_TERMIOS_H
    tcgetattr(0, &term_ios);
    term_ios_orig = term_ios;		/* backup */
    cfmakeraw(&term_ios);
    term_ios.c_lflag |= ISIG;		/* enable signals */
    term_ios.c_cc[VMIN] = 1;		/* return with 1 char */
    term_ios.c_cc[VTIME] = 0;		/* wait infinity */
    tcsetattr(0, TCSANOW, &term_ios);
#else	/* !!HAVE_TERMIOS_H */
    ioctl(0, TIOCGETP, &term_sgtty);
    term_sgtty_orig = term_sgtty;	/* backup */
    term_sgtty.sg_flags |= (CBREAK);
    term_sgtty.sg_flags &= ~(RAW | ECHO | CRMOD | TANDEM);
    ioctl(0, TIOCSETP, &term_sgtty);
#endif	/* !!HAVE_TERMIOS_H */
    out_blocking = 1;
}

/* make terminal input as nowait */
void
scr_term_nowait(void){
    int	arg;
#ifdef	HAVE_TERMIOS_H
    term_ios.c_cc[VMIN] = 0;		/* make input as nowait */
    tcsetattr(0, TCSANOW, &term_ios);
    arg = fcntl(0, F_GETFL, 0);
    arg |= (O_NONBLOCK);
    fcntl(0, F_SETFL, arg);
#else
    arg = fcntl(0, F_GETFL, 0);
    arg |= (FNDELAY);
    fcntl(0, F_SETFL, arg);
#endif	/* !!HAVE_TERMIOS_H */
    out_blocking = 0;
}

/* make terminal input to wait */
void
scr_term_wait(void){
    int	arg;
#ifdef	HAVE_TERMIOS_H
    term_ios.c_cc[VMIN] = 1;		/* make input to wait */
    tcsetattr(0, TCSANOW, &term_ios);
    arg = fcntl(0, F_GETFL, 0);
    arg &= ~(O_NONBLOCK);
    fcntl(0, F_SETFL, arg);
#else
    arg = fcntl(0, F_GETFL, 0);
    arg &= ~(FNDELAY);
    fcntl(0, F_SETFL, arg);
#endif	/* !!HAVE_TERMIOS_H */
    out_blocking = 1;
}

/* make terminal output as blocking mode, and save current mode */
void
scr_term_outblock(void){
    int	arg;

    out_blocking_orig = out_blocking;
#ifdef	HAVE_TERMIOS_H
    arg = out_flags = fcntl(1, F_GETFL, 0);
    arg &= ~(O_NONBLOCK);
    fcntl(1, F_SETFL, arg);
#else
    arg = out_flags = fcntl(1, F_GETFL, 0);
    arg &= ~(FNDELAY);
    fcntl(1, F_SETFL, arg);
#endif	/* !!HAVE_TERMIOS_H */
    out_blocking = 1;
}

/* restore terminal output mode which scr_term_outblock() changed */
void
scr_term_outrestore(void){
#ifdef	HAVE_TERMIOS_H
    fcntl(0, F_SETFL, out_flags);
#else
    fcntl(0, F_SETFL, out_flags);
#endif	/* !!HAVE_TERMIOS_H */
    out_blocking = out_blocking_orig;
}

/* resume terminal to original mode */
void
scr_term_resume(void){
#ifdef	HAVE_TERMIOS_H
    tcsetattr(0, TCSANOW, &term_ios_orig);
#else
    ioctl(0, TIOCSETP, &term_sgtty_orig);
#endif	/* !HAVE_TERMIOS_H */
    out_blocking = 1;
}

/* fflush stdout with blocking terminal output */
void
scr_term_fflush(void){
    if (out_blocking){
	scr_term_outblock();
	fflush(stdout);
	scr_term_outrestore();
    } else {
	fflush(stdout);
    }
}

/*
   scr_pputchar
   replacement of putchar() for tputs()

   NOTE:
    putchar during non-blocking I/O mode may fail, and
    recover from the failuer is very difficult. (Don't forget portability!)
*/
TYPE_TPUTS
scr_pputchar(int c){
    register int	block = 0;

    if (!out_blocking){
	scr_term_outblock();
	block = 1;
    }
    (void) putchar(c);
    if (block)
	scr_term_outrestore();
}

/*
   scr_pmove:
   move cursor of physical screen

   NOTE: this function is called from scr_stopr()

   This function is called from any signal critical/non-critical functions,
   signal blocking in this small function makes overhead.
   So, all functions which calls this function must block signals.

   NON PORTABLE FUNCTION
*/
void
scr_pmove(int y, int x){
    if (scr_py == y && scr_px == x)
	return;		/* nothing to do */

    tputs(tgoto(scr_tc_cm_str, x, y), 1, scr_pputchar);
    scr_term_fflush();
    scr_py = y;
    scr_px = x;
}

/*
   scr_pput:
   put a char onto physical screen
   NOTE: not affect to scr_vx nor scr_vy, but may change scr_px or scr_py

   NON PORTABLE FUNCTION
*/
void
scr_pput(int y, int x, int c){
    ON_CRITICAL;
    scr_pmove(y, x);
    scr_pputchar(c);

    /* I assume cursor will be move to right */
    if (++scr_px >= scr_pw){
	scr_px = -1;		/* on margin is undefined */
    }
    scr_term_fflush();
    OFF_CRITICAL;
}

/*
   scr_clear:
   clear virtual & physical screen

   NON PORTABLE FUNCTION
*/
void
scr_clear(void){
    int	v;

    for (v=0; v<scr_vh; v++){
	memset(scr_vchr[v], (int) ' ', scr_vw);
	(scr_vchr[v])[scr_vw] = '\0';
	memset(scr_vattr[v], (int) SCR_A_CLEAN, scr_vw);
	(scr_vattr[v])[scr_vw] = '\0';
	scr_vlattr[v] = SCR_LA_NONE;
    }
    scr_vx = scr_vy = 0;

    ON_CRITICAL;
    tputs(scr_tc_cl_str, 1, scr_pputchar);
    scr_term_fflush();
    scr_px = scr_py = 0;
    OFF_CRITICAL;
}

/*
   scr_home:
   move cursor to home posision

   NON PORTABLE FUNCTION
*/
void
scr_home(void){
    ON_CRITICAL;
    scr_vx = scr_vy = scr_px = scr_py = 0;
    tputs(scr_tc_ho_str, 1, scr_pputchar);
    scr_term_fflush();
    OFF_CRITICAL;
}

/*
   scr_scroll:
   scroll virtual & physical screen

   NOTE:
     this routine will not touch scr_vx nor scr_vy

   NON PORTABLE FUNCTION
*/
void
scr_scroll(void){
    unsigned char	*ctop, *atop;
    int	v;

    ctop = scr_vchr[0];
    atop = scr_vattr[0];
    for (v=0; v<scr_vh - 1; v++){
	scr_vchr[v] = scr_vchr[v+1];
	scr_vattr[v] = scr_vattr[v+1];
	scr_vlattr[v] = scr_vlattr[v+1];
    }
    memset(ctop, (int) ' ', scr_vw);
    ctop[scr_vw] = '\0';
    scr_vchr[scr_vh - 1] = ctop;
    memset(atop, (int) SCR_A_CLEAN, scr_vw);
    atop[scr_vw] = '\0';
    scr_vattr[scr_vh - 1] = atop;
    scr_vlattr[scr_vh] = SCR_LA_NONE;

    ON_CRITICAL;
    tputs(tgoto(scr_tc_cm_str, 0, scr_ph - 1), 1, scr_pputchar);
    tputs(scr_tc_sf_str, 1, scr_pputchar);
    scr_term_fflush();
    scr_px = scr_py = -1;	/* cursor on physical screen is undefined */
    OFF_CRITICAL;
}

/*
   scr_visible:
   make cursor visible

   NON PORTABLE FUNCTION
*/
void
scr_visible(void){
    if (scr_cur_visible)	/* already visible */
	return;

    scr_cur_visible = 1;
    if (scr_tc_ve_str == NULL)
	return;

    ON_CRITICAL;
    tputs(scr_tc_ve_str, 1, scr_pputchar);
    scr_term_fflush();
    OFF_CRITICAL;
}

/*
   scr_invisible:
   make cursor invisible

   NON PORTABLE FUNCTION
*/
void
scr_invisible(void){
    if (! scr_cur_visible)	/* already invisible */
	return;

    scr_cur_visible = 0;
    if (scr_tc_vi_str == NULL)
	return;
    ON_CRITICAL;
    tputs(scr_tc_vi_str, 1, scr_pputchar);
    scr_term_fflush();
    OFF_CRITICAL;
}

/*
  scr_pbell:
  ring a bell

   NON PORTABLE FUNCTION
*/
void
scr_pbell(void){
    ON_CRITICAL;
    tputs(scr_tc_bl_str, 1, scr_pputchar);
    OFF_CRITICAL;
}

/*
   scr_flush:
   flush dirty chars in virtual screen

   NON PORTABLE FUNCTION
*/
#ifndef	OPT_DELAY_FLUSH
void
scr_flush(void){
#else	/* OPT_DELAY_FLUSH */
# define scr_flush()	/* nothing to do */
void
scr_realflush(void){
#endif	/* OPT_DELAY FLUSH */
    int	v;
    int	sx,ex;
    register int	x;
    register unsigned char	*p;
    int wlen;

    ON_CRITICAL;
    for (v=0; v<scr_vh; v++){
	if (!(scr_vlattr[v] & SCR_LA_DIRTY))
	    continue;		/* the line is clean */
	/* search 1st dirty point */
	p = scr_vattr[v];
	for (x=0; x<scr_vw; x++,p++){
	    if (*p == SCR_A_DIRTY)
		break;
	}
	if (x == scr_vw)
	    continue;		/* no dirty attribute in this line */
	sx = x;
	/* search last dirty point */
	p++;
	ex = x++;
	for (; x<scr_vw; x++,p++){
	    if (*p == SCR_A_DIRTY)
		ex = x;
	}
	/* OK, write from first to last */
	scr_term_outblock();
	scr_pmove(v, sx);
	fwrite(scr_vchr[v]+sx, sizeof(char), ex-sx+1, stdout);
	scr_term_outrestore();
	scr_px = -1;	/* cursor position is undefined */
	memset(scr_vattr[v], (int) SCR_A_CLEAN, scr_vw);
	scr_vlattr[v] &= ~(SCR_LA_DIRTY);
    }
    if (scr_vx != scr_px || scr_vy != scr_py)
	scr_pmove(scr_vy, scr_vx);
    else
	scr_term_fflush();
    OFF_CRITICAL;
}

/*
   scr_redraw:
   redraw screen to make sure be same as virtual one

   NOTE: this functions is called from scr_stopr()

   NON PORTABLE FUNCTION
*/
void
scr_redraw(void){
    register unsigned char	*p;
    register int	x;
    int	v;
    unsigned char	*sp, *ep;
    int sx,len;
    int wlen;

    if (! scr_in_signal)
	ON_CRITICAL;
    tputs(scr_tc_cl_str, 1, scr_pputchar);	/* clear screen */
    for (v=0; v<scr_vh; v++){
	/* search first char in the current line */
	for (x=0,p=scr_vchr[v]; x<scr_vw; x++,p++){
	    if (*p != ' ')
		break;
	}
	if (x == scr_vw)
	    continue;		/* blank line */
	sx = x;			/* save 1st point of char */
	sp = p;
	/* search end of char */
	ep = ++p;
	for (x++; x<scr_vw; x++,p++){
	    if (*p != ' ')
		ep = p;
	}
	scr_term_outblock();
	scr_pmove(v, sx);
	fwrite(sp, sizeof(char), ep-sp+1, stdout);
	scr_term_outrestore();
	memset(scr_vattr[v], (int) SCR_A_CLEAN, scr_vw);
	scr_vlattr[v] &= ~(SCR_LA_DIRTY);
    }
    scr_pmove(scr_vy, scr_vx);
    if (! scr_in_signal)
	OFF_CRITICAL;
}

/*
   scr_vright:
   move virtual cursor to right
*/
void
scr_vright(int flag){
    if (++scr_vx >= scr_vw){
	scr_vx = 0;
	if (++scr_vy >= scr_vh){
	    scr_vy = scr_vh - 1;
	    scr_scroll();
	}
    }
    if (scr_cur_visible && (flag & SCR_F_IMM)){
	ON_CRITICAL;
	scr_pmove(scr_vy, scr_vx);
	OFF_CRITICAL;
    }
}

/*
   scr_vleft:
   move virtual cursor to left
*/
void
scr_vleft(int flag){
    if (--scr_vx < 0){
	scr_vx = scr_vw - 1;
	if (--scr_vy < 0){
	    /* already upper-left of screen */
	    scr_vx = 0;
	    scr_vy = 0;
	}
    }
    if (scr_cur_visible && (flag & SCR_F_IMM)){
	ON_CRITICAL;
	scr_pmove(scr_vy, scr_vx);
	OFF_CRITICAL;
    }
}

/*
   scr_vup:
   move virtual cursor to right
*/
void
scr_vup(int flag){
    if (--scr_vy < 0){
	scr_vy = 0;
    }
    if (scr_cur_visible && (flag & SCR_F_IMM)){
	ON_CRITICAL;
	scr_pmove(scr_vy, scr_vx);
	OFF_CRITICAL;
    }
}

/*
   scr_vdown:
   move virtual cursor to down
*/
void
scr_vdown(int flag){
    if (++scr_vy >= scr_vh){
	scr_vy = scr_vh - 1;
	scr_scroll();
    }
    if (scr_cur_visible && (flag & SCR_F_IMM)){
	ON_CRITICAL;
	scr_pmove(scr_vy, scr_vx);
	OFF_CRITICAL;
    }
}

/*
  scr_vinsline:
  insert a blank line

  NOTE:
    This function can optimize with "al" termcap entry.
    However, this simple approach is more portable and
    async-update friendly.
*/
void
scr_vinsline(int iy,int flag){
    int	x,y;
    unsigned char	c;
    unsigned char	*cp,*ap;

    /* scroll down */
    for (y=scr_vh-1; y>iy; y--){
	scr_vlattr[y] = scr_vlattr[y-1];
	for (x=0; x<scr_vw; x++){
	    if ((c = scr_vchr[y-1][x]) != scr_vchr[y][x]){
		scr_vchr[y][x] = c;
		scr_vattr[y][x] = SCR_A_DIRTY;
		scr_vlattr[y] |= SCR_LA_DIRTY;
	    }
	}
    }

    /* clear new line */
    cp = scr_vchr[iy];
    ap = scr_vattr[iy];
    for (x=0 ; x < scr_vw; x++, cp++, ap++){
	if (*cp != ' '){
	    *cp = ' ';
	    *ap = SCR_A_DIRTY;
	}
    }
    /* line is dirty in birth */
    scr_vlattr[iy] = SCR_LA_DIRTY;

    if (flag & SCR_F_IMM)
	scr_flush();
}

/*
   scr_vcrlf:
   CR/LF on virtual screen
*/
void
scr_vcrlf(int flag){
    scr_vx = 0;
    while((scr_vy < scr_vh-1) && (scr_vlattr[scr_vy] & SCR_LA_CONT))
	scr_vdown(flag);
    scr_vdown(flag);
}

/*
   scr_vputc:
   put a char to virtual screen

   NOTE:
    If put into more right char, the line joined to next line.
*/
void
scr_vputc(unsigned char c, int flag){
    int	oy;

    /* put only if character modified */
    if (scr_vchr[scr_vy][scr_vx] != c){
	scr_vchr[scr_vy][scr_vx] = c;		/* put onto virtual screen */
	/* put onto physical screen or mark it as dirty for later update */
	if (flag & SCR_F_IMM){
	    scr_pput(scr_vy, scr_vx, c);
	    scr_vattr[scr_vy][scr_vx] = SCR_A_CLEAN;
	} else {
	    scr_vattr[scr_vy][scr_vx] = SCR_A_DIRTY;
	    scr_vlattr[scr_vy] |= SCR_LA_DIRTY;
	}
    }

    /* move cursor */
    if (++scr_vx >= scr_vw){		/* take over the line */
	scr_vx = 0;
	oy = scr_vy;
	if (++scr_vy < scr_vh){
#if 0
	    /* NOTE: many graphical applications (likes ELFES2) expects
		putc on last point cause no effect */
	    /* not last line, so insert a empty line */
	    if (!(scr_vlattr[oy] & SCR_LA_CONT)){ /* make as a continous line */
	        scr_vinsline(scr_vy,flag);
		scr_vlattr[oy] |= SCR_LA_CONT;
	    }
#endif
	    /* not last line, so join the line */
	    scr_vlattr[oy] |= SCR_LA_CONT;
	} else {
	    /* last line, so scroll */
	    scr_vy = scr_vh - 1;
	    scr_scroll();
	    scr_vlattr[oy-1] |= SCR_LA_CONT;
	}
    }

    if (scr_cur_visible && (flag & SCR_F_IMM)){
	ON_CRITICAL;
	scr_pmove(scr_vy, scr_vx);
	OFF_CRITICAL;
    }
}

/*
   scr_vkill:
   kill all chars of current line group after current cursor posion
   killed text is saved to cut buffer

   NOTE:
     This function can optimize with "ce" termcap entry
*/
scr_vkill(int flag){
    int	x,y;
    unsigned char *ap, *cp;
    unsigned char *cutp, *cutlast;

    x = scr_vx;
    cutp = cutlast = scr_cutbuf;
    for (y=scr_vy; y < scr_vh; y++){
	cp = scr_vchr[y]+x;
	ap = scr_vattr[y]+x;
	for (;x < scr_vw; x++, cp++, ap++, cutp++){
	    if ((*cutp = *cp) != ' '){
		*cp = ' ';
		*ap = SCR_A_DIRTY;
		cutlast = cutp + 1;
	    }
	}
	scr_vlattr[y] |= SCR_LA_DIRTY;
	if (!(scr_vlattr[y] & SCR_LA_CONT))
	    break;		/* finished */
	x = 0;
    }

    *cutlast = '\0';		/* terminate cut buffer */

    if (flag & SCR_F_IMM)
	scr_flush();
}

/*
  scr_delete:
  delete a char of current cursor position and shift a line group to left
*/
void
scr_delete(flag){
    int	x,y;
    register unsigned char *src,*dst;
    unsigned char *ap;
    unsigned char c;

    x = scr_vx;
    for (y=scr_vy; ; y++){
	dst = scr_vchr[y] + x;
	src = dst + 1;
	ap = scr_vattr[y] + x;
	for (x++; x<scr_vw; x++){	/* x means src point */
	    if (*dst != *src){
		*dst = *src;
		*ap = SCR_A_DIRTY;
	    }
	    dst++;
	    src++;
    	    ap++;
	}
	scr_vlattr[y] |= SCR_LA_DIRTY;
	if (!(scr_vlattr[y] & SCR_LA_CONT) || y>=scr_vh-1)
	    break;			/* not joined */
	/* copy first char of next line to last point of current line */
	if (*dst != (c = scr_vchr[y+1][0])){
	    *dst = c;
	    *ap = SCR_A_DIRTY;
	}
	x = 0;
    }
    /* clear last char */
    if (*dst != ' '){
	*dst = ' ';
        *ap = SCR_A_DIRTY;
    }

    if (flag & SCR_F_IMM)
	scr_flush();
}

/*
  scr_backspace:
  delete a left char of current cursor position and shift left
*/
void
scr_backspace(int flag){
    int	oy;

    oy = scr_vy;
    scr_vleft(SCR_F_NONE);
    if (oy != scr_vy){		/* backspace over lines */
	scr_vlattr[scr_vy] |= SCR_LA_CONT;
    }
    scr_delete(flag);
}

/*
  scr_insert:
  insert 'num' spaces into current cursor position

  NOTE:
   This routine doesn't clear inserted spaces.  garbage will be remain.
*/
void
scr_insert(int num, int flag){
    int x,y;
    unsigned char *cp,*ap;
    unsigned char c,bc;
    int dirty;

    if (num <= 0)
	return;

    for (; num>0; num--){
	x = scr_vx;
	y = scr_vy;			/* start from current cursor */
	bc = ' ';			/* insert char */
	for (y=scr_vy; y<scr_vh; y++){
	    dirty = 0;
	    cp = scr_vchr[y] + x;
	    ap = scr_vattr[y] + x;
	    for (; x<scr_vw; x++,cp++,ap++){
		if ((c = *cp) != bc){
		    *cp = bc;
		    bc = c;
		    *ap = SCR_A_DIRTY;
		    dirty = 1;
		}
	    }
	    if (dirty)
		scr_vlattr[y] |= SCR_LA_DIRTY;
	    if (scr_vlattr[y] & SCR_LA_CONT){
		x = 0;
		continue;	/* cont. line go next line */
	    }
	    if ((bc == ' ') && !((scr_vx == scr_vw-1) && (scr_vy == y)))
		break;		/* no need to join nor shift to next line */
	    /* line joined! */
	    scr_vlattr[y] |= SCR_LA_CONT;	/* mark as continuous line */
	    if (y+1 < scr_vh){
	        scr_vinsline(y+1,flag);		/* insert a new line */
	    } else {
		scr_scroll();
		scr_vy--;
		y--;
	    }
	    x = 0;
	}
    }
    if (flag & SCR_F_IMM)
	scr_flush();
}

/*
  scr_insch:
  insert a char into current cursor position
*/
void
scr_insch(unsigned char c, int flag){
    int	oy;

    scr_insert(1, SCR_F_NONE);
    scr_vputc(c, SCR_F_NONE);

    if (flag & SCR_F_IMM)
	scr_flush();
}

/*
  scr_top:
  move cursor onto top position of current line group
*/
void
scr_top(int flag){
    int	y;

    /* search 1st line of current line group */
    for (y=scr_vy-1; y>=0; y--){
	if (!(scr_vlattr[y] & SCR_LA_CONT))
	    break;
	scr_vy = y;
    }
    scr_vx = 0;

    /* apply to screen if need */
    if (scr_cur_visible && (flag & SCR_F_IMM)){
	ON_CRITICAL;
	scr_pmove(scr_vy, scr_vx);
	OFF_CRITICAL;
    }
}

/*
  scr_end:
  move cursor to end position of current line group

  NOTE:
    'end' position is right char of last visible char.  This choice makes
    user can append text easily.
    Only when no visible char in current line group, move to most right
    position of last line of the group.
*/
void
scr_end(int flag){
    register int	x;
    register unsigned char	*cp;
    int y;
    int min_y,max_y;
    int	found;

    /* search 1st line of current line group */
    min_y = scr_vy;
    for (y=scr_vy-1; y>=0; y--){
	if (!(scr_vlattr[y] & SCR_LA_CONT))
	    break;
	min_y = y;
    }

    /* search last line of current line group */
    for (y=scr_vy; y<scr_vh; y++){
	max_y = y;
	if (!(scr_vlattr[y] & SCR_LA_CONT))
	    break;
    }

    /* search last printable char in the line group */
    found = 0;
    for (y=max_y; y>=min_y; y--){
	cp = scr_vchr[y] + scr_vw - 1;
	for (x=scr_vw-1; x>=0; x--, cp--){
	    if (*cp != ' '){
		found = 1;
		break;
	    }
	}
	if (found)
	    break;
    }
    if (found){
	if ((scr_vx = x + 1) >= scr_vw){	/* step out right corner */
	    if (y < max_y){		/* there is next line in group */
		y++;
		scr_vx = 0;
	    } else {			/* this is last line in group */
		scr_vx = scr_vw - 1;
	    }
	}
	scr_vy = y;
    } else {
	/* completly space line group */
	scr_vx = scr_vw - 1;
	scr_vy = max_y;
    }

    /* apply to screen if need */
    if (scr_cur_visible && (flag & SCR_F_IMM)){
	ON_CRITICAL;
	scr_pmove(scr_vy, scr_vx);
	OFF_CRITICAL;
    }
}

/*
  scr_yank:
  insert contents of cut buffer into current cursor position.
*/
void
scr_yank(int flag){
    register unsigned char *p;

    p = scr_cutbuf;
    while(*p != '\0'){
	if (scr_mode_insert)
	    scr_insch(*p, SCR_F_NONE);
	else
	    scr_vputc(*p, SCR_F_NONE);
	p++;
    }
    if (flag & SCR_F_IMM)
	scr_flush();
}

/*
   fix x and y value to fit virtual screen.
*/
void
scr_fixxy(int *y, int *x){
    if (x != NULL){
	if (*x >= scr_vw){
	    *x = scr_vw;
	} else if (*x < 0){
	    *x = 0;
	}
    }
    if (y != NULL){
	if (*y >= scr_vh){
	    *y = scr_vh;
	} else if (*y < 0){
	    *y = 0;
	}
    }
}

/*
   scr_sostounix:
   convert SWORD S-OS character set to ASCII character set

   XXX: simple but not enough, may be table
*/
unsigned char
scr_sostoascii(unsigned char c){
    if (c > 0xa0){		/* 7bit hack */
	c = '*';
    }
    return(c);
}

/*
   addch() with SWORD screen control
*/
void
scr_putch(unsigned char c, int flag){
    int	x,y;

    switch(c){
      case 0:			/* NULL */
	return;
      case 0x0c:		/* CLS */
	scr_clear();
	return;
      case '\r':		/* CR */
      case '\n':
	scr_vcrlf(flag);
	return;
      case 0x1c:		/* right */
	scr_vright(flag);
	return;
      case 0x1d:		/* left */
	scr_vleft(flag);
	return;
      case 0x1e:		/* up */
	scr_vup(flag);
	return;
      case 0x1f:		/* down */
	scr_vdown(flag);
	return;
    }
    c = scr_sostoascii(c);
    if (c < 0x20){		/* control hack */
	return;
    }

    scr_vputc(c, flag);
}

/*
   convert 1char from UNIX key to SWORD input
*/
int
scr_conv(char oc){
    int	c;

    /* some system thinks char as signed char */
    c = ((unsigned char) oc) & 0xff;

    switch(c){
	/* C-c maps to SOS Break code */
	case 'C'-'@':
	   c = SCR_BREAK;
	   break;
    }

    if (scr_capson){
	if (islower(c)){
	    c = toupper(c);
	} else if (isupper(c)){
	    c = tolower(c);
	}
    }
    return(c);
}

/*
    scr_winkey:
    get 1char (wait until input)

    NON PORTABLE FUNCTION
*/
int scr_winkey(void){
    char	c;

    scr_visible();
    scr_term_wait();		/* make input to wait */
    while (read(0, &c, 1) <= 0)
	;		/* wait until read something */
    c = scr_conv(c);
    if (c == SCR_BREAK)
	breaked = 0;
    return(c);
}



/****************************************
  exported functions
*****************************************/

/*
   initialize screen module
    NON PORTABLE FUNCTION
*/
int
scr_initx(void){
    char	bp[1024];
    char	cp[1024],*cpp;
    int		y;
    struct winsize	ws;
    struct sigaction	sact;

    if (getenv("TERM") == NULL || tgetent(bp, getenv("TERM")) <= 0){
#if OPT_DEFAULT_ANSI
	if (setenv("TERMCAP", "ansi:sf=^J:cl=\E[H\E[J:cm=\E[%i%d;%dH:ho=\E[H:bl=^G:li#25:co#80:", 1)){
	    fprintf(stderr,"scr_initx: Can't set TERMCAP environment.\n");
	    return(1);
	}
	if (tgetent(bp, "ansi") <= 0)
#endif
	    {
		fprintf(stderr,"scr_initx: Can't get termcap entry.\n");
		return(1);
	    }
    }

    /* scroll text up */
    cpp = cp;
    if (tgetstr("sf", &cpp) == NULL){
	scr_tc_sf_str = "\n";		/* ANSI-C */
    } else {
	scr_tc_sf_str = strdup(cp);
    }

    /* clear screen & home cursor */
    cpp = cp;
    if (tgetstr("cl", &cpp) == NULL){
	fprintf(stderr,"scr_initx: terminal has no cl capability.\n");
	return(1);
    }
    scr_tc_cl_str = strdup(cp);

    /* move cursor */
    cpp = cp;
    if (tgetstr("cm", &cpp) == NULL){
	fprintf(stderr,"scr_initx: terminal has no cm capability.\n");
	return(1);
    }
    scr_tc_cm_str = strdup(cp);

    /* home cursor */
    cpp = cp;
    if (tgetstr("ho", &cpp) == NULL){
	fprintf(stderr,"scr_initx: terminal has no ho capability.\n");
	return(1);
    }
    scr_tc_ho_str = strdup(cp);

    /* bell */
    cpp = cp;
    if (tgetstr("bl", &cpp) == NULL){
	scr_tc_bl_str = "\a";	/* ANSI-C */
    } else {
	scr_tc_bl_str = strdup(cp);
    }

    /* cursor invisible (optional) */
    cpp = cp;
    if (tgetstr("vi", &cpp) == NULL){
	scr_tc_vi_str = NULL;
    } else {
	scr_tc_vi_str = strdup(cp);
    }

    /* cursor appear normal (optional) */
    cpp = cp;
    if (tgetstr("ve", &cpp) == NULL){
	scr_tc_ve_str = NULL;
	if (scr_tc_vi_str != NULL){	/* can't restore... disable */
	    free(scr_tc_vi_str);
	    scr_tc_vi_str = NULL;
	}
    } else {
	scr_tc_ve_str = strdup(cp);
    }

    /* get window size */
#ifdef	TIOCGWINSZ
    /* NOTE: TIOCGWINSG is not portable, but best way to get screen size */
    /* NOTE: some environments (includes terminal emulator mode in mule) has
             TIOCGWINSZ but return 0.  Then ignore this illeagal answer */
    if ((ioctl(0, TIOCGWINSZ, &ws) != -1) && ws.ws_row && ws.ws_col){
	scr_ph = ws.ws_row;
	scr_pw = ws.ws_col;
    } else
#endif
    {
	scr_ph = tgetnum("li");
	scr_pw = tgetnum("co");
    }

    /* check screen size */
    if ((scr_vw = EM_WIDTH) > scr_pw){
	/* XXX: We can shrink width to 40 instead of 80,
	        But must cooperate with #WIDTH in trap.c */
	fprintf(stderr,"I need %d width on screen, found only %d width\n",
		scr_vw, scr_pw);
	return(1);
    }
    if ((scr_vh = EM_MAXLN) > scr_ph){
	fprintf(stderr,"I need %d lines on screen, found only %d lines\n",
		scr_vh, scr_ph);
	return(1);
    }

    /* make raw+signal mode */
    scr_term_makeraw();

    /* allocate buffer */
    for (y=0; y<SCR_MAXLINES; y++){
	if (scr_vchr[y] != NULL)
	    free(scr_vchr[y]);
	if (scr_vattr[y] != NULL)
	    free(scr_vattr[y]);
	scr_vchr[y] = (unsigned char *)malloc(SCR_MAXWIDTH + 1);
	scr_vattr[y] = (unsigned char *)malloc(SCR_MAXWIDTH + 1);
	if (scr_vchr[y] == NULL || scr_vattr[y] == NULL){
	    perror("malloc");
	    return(1);
	}
    }
    scr_clear();

    /* clear keymap */
    scr_mapclear();

    /* set signal handler */
    /* create suspend signal set for critical region barrier */
    sigemptyset(&scr_intset);
    sigaddset(&scr_intset, SIGTSTP);
    /* interrupt (C-c in BSD) */
    sact.sa_handler = scr_intr;
    sigemptyset(&(sact.sa_mask));
    sact.sa_flags = 0;
    (void) sigaction(SIGINT, &sact, NULL);
    /* suspend (C-z in BSD) */
    sact.sa_handler = scr_stopr;
    sigemptyset(&(sact.sa_mask));
    sact.sa_flags = 0;
    (void) sigaction(SIGTSTP, &sact, NULL);
#ifdef	OPT_DELAY_FLUSH
    /* alarm */
    sigaddset(&scr_intset, SIGALRM);
    sact.sa_handler = scr_alrm;
    sigemptyset(&(sact.sa_mask));
    sigaddset(&(sact.sa_mask), SIGTSTP);
    sact.sa_flags = 0;
    (void) sigaction(SIGALRM, &sact, NULL);
#endif

#ifdef	OPT_DELAY_FLUSH
    {
	struct itimerval interval;

	interval.it_interval.tv_sec = 0;
	interval.it_interval.tv_usec = OPT_DELAY_FLUSH_TIME;
	interval.it_value = interval.it_interval;
	(void) setitimer(ITIMER_REAL, &interval, NULL);
    }
#endif

    return(0);
}

/*
  scr_finish:
  terminate screen module
    NON PORTABLE FUNCTION
*/
int
scr_finish(void){
    scr_term_resume();
    return(0);
}

/*
   scr_caps:
   control caps lock

   s is boolean value, caps-on if true
*/
void
scr_caps(int s){
    scr_capson = s;
}

/*
  scr_putchar:
  put a S-OS char onto screen
*/
void scr_putchar(char c){
    scr_putch(c, SCR_F_IMM);
}

/*
    scr_asyncputchar:
    scr_putchar without buffer flush
*/
void
scr_asyncputchar(char c){
    scr_putch(c, SCR_F_NONE);
}

/*
    scr_sync:
    pair of scr_asyncputchar
*/
void
scr_sync(void){
    scr_flush();
}

void scr_ltnl(void){
    scr_putch('\r', SCR_F_IMM);
}

void scr_nl(void){
    if (scr_vx != 0){
	scr_vcrlf(SCR_F_IMM);
    }
}

void scr_puts(char *buf){
    while(*buf != '\0')
	scr_putch(*buf++, SCR_F_NONE);
    scr_flush();
}

void scr_tab(int x){
    int	i;

    for (i = x - scr_vx; i > 0; i--){
	scr_putch(' ', SCR_F_NONE);
    }
    scr_flush();
}

/*
   control key functions
*/
void
scr_key_backspace(void){
    scr_backspace(SCR_F_IMM);
}
void
scr_key_delete(void){
    scr_delete(SCR_F_IMM);
}
void
scr_key_top(void){
    scr_top(SCR_F_IMM);
}
void
scr_key_end(void){
    scr_end(SCR_F_IMM);
}
void
scr_key_up(void){
    scr_vup(SCR_F_IMM);
}
void
scr_key_down(void){
    scr_vdown(SCR_F_IMM);
}
void
scr_key_forward(void){
    scr_vright(SCR_F_IMM);
}
void
scr_key_back(void){
    scr_vleft(SCR_F_IMM);
}
void
scr_key_redraw(void){
    scr_redraw();
}
void
scr_key_kill(void){
    scr_vkill(SCR_F_IMM);
}
void
scr_key_tab(void){
    int	x;

    x = (((scr_vx / SCR_TABLEN) + 1) * SCR_TABLEN) - scr_vx;
    if (scr_vx + x >= scr_vw)
	return;
    scr_insert(x, SCR_F_NONE);
    scr_vx += x;
    scr_flush();
}
void
scr_key_yank(void){
    scr_yank(SCR_F_IMM);
}
void
scr_key_imode(void){
    scr_mode_insert = (scr_mode_insert) ? 0 : 1;
}
void
scr_key_clear(void){
    scr_clear();
}

/*
   line input & return length of input

   NOTE:
    Most screen editor on S-OS assume that getl routine has enough
    screen edit capability.

   XXX: control key binding must be customize.
*/
int scr_getl(char *buf){
    int	c;
    int	x,bx,mx,y;

    scr_visible();
    /* screen move */
    while((c = scr_flget()) != '\r' && c != '\n' && c != SCR_BREAK){
	if (c < 0)
	    break;	/* XX: EOF??? */
	if (c < ' '){
	    /* control code */
	    if (keymap[c] >= 0)
		(keyfuncs[keymap[c]].func)();
	} else {
	    if (scr_mode_insert)
		scr_insch(scr_sostoascii(c), SCR_F_IMM);
	    else
		scr_vputc(scr_sostoascii(c), SCR_F_IMM);
	}
    }
    if (c == SCR_BREAK){	/* break */
	buf[0] = SCR_BREAK;
	buf[1] = '\0';
	return(1);
    }

    /* get a line */
    mx = -1;		/* point of last visible char */
    bx = 0;
    /* search starting line of current continous lines group */
    for (y=scr_vy; y>0; y--){
	if (!(scr_vlattr[y-1] & SCR_LA_CONT))
	    break;		/* not joined */
    }
    /* copy from screen to buffer */
    for (; y<scr_vh; y++){
	for (x=0; x<scr_vw; x++,bx++){
	    if ((buf[bx] = scr_vchr[y][x]) != ' '){
		    mx = bx;
	    }
	}
	if (!(scr_vlattr[y] & SCR_LA_CONT))
	   break;
    }
    buf[++mx] = '\0';
    scr_vcrlf(SCR_F_IMM);
    return(mx);
}

/*
   get a key datum, not wait even if no datum
    NON PORTABLE FUNCTION
*/
int scr_getky(void){
    char	c;

    /* XXX: this is simple, but brain-damaged algorithm in multitask system */
    /* we need some tricks such as constat() in yaze:bios.c */

    scr_invisible();
    scr_term_nowait();			/* make input as nowait */
    if (read(0, &c, 1) > 0){
	c = scr_conv(c);
	if (c == SCR_BREAK){
	    if (breaked)		/* code inserted by scr_intr() */
		breaked = 0;
	    else			/* ESC key, so tell scr_brkey */
		breaked = 1;
	}
	scr_term_wait();
	return(c);
    } else {
	scr_term_wait();
	return(0);
    }
}

/* return TRUE if break */
int scr_brkey(void){
    int	c;

    if (! breaked)
	return(0);

    /* already pressed break key, so eat up till break code */
    while((c = scr_getky()) != SCR_BREAK && c != '\0')
	; /* eat up */
    breaked = 0;
    return(1);
}

int scr_inkey(void){
    scr_invisible();
    return(scr_winkey());
}

/*
   pause if space bar pressed
   return TRUE if break
*/
int scr_pause(void){
    int	c;

    /* eat up input queue */
    while((c = scr_getky()) != 0 &&
	  c != ' ' &&
	  c != SCR_BREAK)
	/* nothing to do */
	;

    switch(c){
      case 0:		/* no more input */
	return(0);
      case ' ':		/* space */
	scr_visible();
	if (scr_inkey() == SCR_BREAK)	/* wait a key press & check break */
	    return(1);
	return(0);
      case SCR_BREAK:	/* break key */
	return(1);
    }

    /* not reach */
    return(0);
}

void scr_bell(void){
    scr_pbell();
}

void scr_csr(int *y, int *x){
    *y = scr_vy;
    *x = scr_vx;
}

int scr_scrn(int y, int x){
    scr_fixxy(&y, &x);

    return(scr_vchr[y][x]);
}

void scr_loc(int y, int x){
    scr_fixxy(&y, &x);

    scr_vy = y;
    scr_vx = x;
#ifndef	OPT_DELAY_FLUSH
    if (scr_cur_visible){	/* move cursor only if cursor is visible */
	ON_CRITICAL;
	scr_pmove(y, x);
	OFF_CRITICAL;
    }
#endif
}

int scr_flget(void){
    scr_visible();
    return(scr_winkey());
}

void scr_width(int x){
    if ((scr_pw >= x) && (SCR_MAXWIDTH >= x)){
	scr_vw = x;
    }
    scr_clear();
}


/****************************************
  interrupt handers
*****************************************/

/*
   interrupt signal handler

   handle interrupt signal, this means break key pressed

    NON PORTABLE FUNCTION
*/
RETSIGTYPE
scr_intr(int sig){
    struct sigaction	sact;
    int	what;
    char	c;

    breaked = 1;		/* mark as pressed */

    /* remove input queue */
#ifdef	HAVE_TERMIOS_H
    tcflush(0, TCIFLUSH);
#else
    what = FREAD;
    ioctl(0, TIOCFLUSH, &what);
#endif

#ifdef TIOCSTI
    /* push break key symbol */
    c = SCR_BREAK;
    ioctl(0, TIOCSTI, &c);		/* XXX: not portable */
#endif

    /* on some systems, we may have to re-install handler */
    /* I can not found handler must re-install or not in POSIX standard */
    sact.sa_handler = scr_intr;
    sigemptyset(&(sact.sa_mask));
    sact.sa_flags = 0;
    (void) sigaction(SIGINT, &sact, NULL);
}

/*
   stop signal handler

   handle stop signal, it may suspend process

   NOTE:
     There are many trouble to use interrupt with buffering I/O.
     The safe way is that the interrupt handler only set flag, and
     detect & suspend in some major routine such as inkey or getky.
     This safe solution can cause simple problem: can't suspend in
     busy loop.
     Is the best solution is check the suspend flag in z80loop()?

     In this routine, flush stdout buffer by scr_pmove() first.
     All routines which uses stdout blocks this signal.
     This two tricks makes problem minimum.

    NON PORTABLE FUNCTION
*/
RETSIGTYPE
scr_stopr(int sig){
    struct sigaction	sact;

    scr_in_signal = 1;

    /* set screen & terminal to normal state */
    scr_pmove(scr_ph - 1, 0);		/* set cursor to bottom of screen */
    scr_term_resume();			/* resume terminal mode */

    /* suspend myself */
    (void) kill(getpid(), SIGSTOP);

    /* resume to S-OS mode */
    scr_term_makeraw();			/* set terminal mode to raw again */
    scr_redraw();			/* refresh screen */

    scr_in_signal = 0;

    /* on some systems, we may have to re-install handler */
    /* I can not found handler must re-install or not in POSIX standard */
    sact.sa_handler = scr_stopr;
    sigemptyset(&(sact.sa_mask));
    sact.sa_flags = 0;
    (void) sigaction(SIGTSTP, &sact, NULL);
}

#ifdef	OPT_DELAY_FLUSH
RETSIGTYPE
scr_alrm(int sig){
    struct sigaction	sact;

    scr_in_signal = 1;

    scr_realflush();			/* refresh screen */

    scr_in_signal = 0;

    /* on some systems, we may have to re-install handler */
    /* I can not found handler must re-install or not in POSIX standard */
    sact.sa_handler = scr_alrm;
    sigemptyset(&(sact.sa_mask));
    sigaddset(&(sact.sa_mask), SIGTSTP);
    sact.sa_flags = 0;
    (void) sigaction(SIGALRM , &sact, NULL);
}
#endif

/*
   manipulate key mappings
*/
/*
   clear current key mapping
*/
void
scr_mapclear(void){
    int	i;

    for (i=0; i<(int)' '; i++)
	keymap[i] = -1;
}
/*
   add mapping
   funcname == NULL means unmap

   return 0 if success
     SCR_MAPERR_CODE ... invalid key code
     SCR_MAPERR_FUNC ... invalid function name

*/
int
scr_mapadd(char code, char *funcname){
    int	n;

    if (code < 0 || code > ' ')
	return(SCR_MAPERR_CODE);

    if (funcname == NULL){
	keymap[code] = -1;
	return(0);
    }

    for (n=0; keyfuncs[n].funcname != NULL; n++){
	if (strcmp(keyfuncs[n].funcname, funcname) == 0){
	    keymap[code] = n;
	    return(0);
	}
    }
    return(SCR_MAPERR_FUNC);
}
/*
   lookup current mapping

   return a pointer to function name, or NULL if not bind
*/
char *
scr_maplook(char code){
    if (keymap[code] < 0)
	return(NULL);
    return(keyfuncs[keymap[code]].funcname);
}
