/*
   SWORD Emurator
   $Id: sos.c,v 1.9 1999/02/19 16:50:10 tate Exp tate $

   tate@spa.is.uec.ac.jp
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "compat.h"
#include "simz80.h"
#include "keymap.h"
#include "sos.h"
#include "dio.h"
#include "util.h"
#include "screen.h"
#include "trap.h"

#ifndef VERSION
#define VERSION	"0.5 (beta)"		/* version */
#endif

#ifndef	DOSFILE
# define DOSFILE	"dos.bin"
#endif

#ifndef	RCFILE
# define RCFILE		".sosrc"
#endif

/* command processor state */
#define	CCP_MODE_NONE	(0)
#define	CCP_MODE_INIT	(1)	/* in initialize state, don't call other module */

/* Z80 registers */
WORD af[2];			/* accumulator and flags (2 banks) */
int af_sel;			/* bank select for af */

struct ddregs regs[2];		/* bc,de,hl */
int regs_sel;			/* bank select for ddregs */

WORD ir;			/* other Z80 registers */
WORD ix;
WORD iy;
WORD sp;
WORD pc;
WORD IFF;

BYTE ram[EM_MEMAX + 1];		/* Z80 memory space */

static char *progname;
char	*dosfile = NULL;	/* common DOS image file */

/* getopt declarations */
extern int getopt();
extern char *optarg;
extern int optind, opterr, optopt;

/*
   quit emulator
*/
void
emu_quit(void){
    (void) scr_finish();
    exit(0);
}

/*
   SWORD command line interpriter

   return true if quit command requested
*/
int
ccpline(char *p, int mode){
    char lbuf[CCP_LINLIM];
    char *np,c;
    char *cp;
    int n;

    /* prompt & space skip */
    while((c = *p) == '$' || c == ' ')
	p++;
    if (isupper(c))
	c = tolower(c);
    np = strtok(p, " ");	/* np := command name */
    if (c == 'r' || c == 'q'){	/* return to emulator */
	return(1);
    } else if (c == '#' || c == '\0'){
	/* comment or empty */
	return(0);
    } else if (strcasecmp(np, "chdir") == 0 ||
	       strcasecmp(np, "cd") == 0){
	if ((np = strtok(NULL, " ")) != NULL){
	    if (chdir(np)){
		    snprintf(lbuf, CCP_LINLIM, "%s: %s\r", np, strerror(errno));
		scr_puts(lbuf);
	    }
	}
	if (getcwd(lbuf, sizeof(lbuf)) != NULL){
	    scr_puts(lbuf);
	    scr_nl();
	} else {
	    scr_puts(strerror(errno));
	    scr_nl();
	}
    } else if (strcasecmp(np, "dosfile") == 0){
	if (mode == CCP_MODE_INIT){
	    if ((np = strtok(NULL, " ")) != NULL)
		dosfile = strdup(np);
	}
	if (dosfile != NULL){
		snprintf(lbuf, CCP_LINLIM, "<%s> is current dos image file\r", dosfile);
	    scr_puts(lbuf);
	}
    } else if (strcasecmp(np, "mount") == 0){
	if ((np = strtok(NULL, " ")) == NULL){
	    for (n=0; n<SOS_MAXIMAGEDRIVES; n++){
		if (dio_disk[n] != NULL){
			snprintf(lbuf, CCP_LINLIM,"disk#%d : %s\r", n, dio_disk[n]);
		    scr_puts(lbuf);
		} else {
			snprintf(lbuf, CCP_LINLIM, "disk#%d : not mounted.\r", n);
		    scr_puts(lbuf);
		}
	    }
	    return(0);
	}
	n = atoi(np);
	if (n < 0 || n >= SOS_MAXIMAGEDRIVES){
	    scr_puts("bad drive number\r");
	    return(0);
	}
	if ((np = strtok(NULL, " ")) == NULL){
	    if (dio_disk[n] != NULL){
		    snprintf(lbuf, CCP_LINLIM,
			"unmount <%s> as disk#%d\r",dio_disk[n],n);
		scr_puts(lbuf);
		dio_diclose(n);
		free(dio_disk[n]);
		dio_disk[n] = NULL;
	    } else {
		    snprintf(lbuf, CCP_LINLIM, "disk#%d : not mounted.\r",n);
		    scr_puts(lbuf);
	    }
	    return(0);
	}
	dio_diclose(n);
	free(dio_disk[n]);
	if ((dio_disk[n] = strdup(np)) == NULL){
	    scr_puts(strerror(errno));
	    scr_nl();
	    return(0);
	}
	snprintf(lbuf, CCP_LINLIM, "<%s> mounted as disk#%d\r", dio_disk[n],n);
	scr_puts(lbuf);
    } else if (strcasecmp(np, "keymap") == 0){
	if ((np = strtok(NULL, " ")) == NULL){
	    scr_puts("Current bindings:\r");
	    for (n=0; n<(int)' '; n++){
		if ((cp = scr_maplook(n)) != NULL){
			snprintf(lbuf, CCP_LINLIM, "C-%c: %s\r", n+'`', cp);
		    scr_puts(lbuf);
		}
	    }
	    return(0);
	}
	cp = np;
	if ((np = strtok(NULL, " ")) == NULL){
	    scr_puts("must specify corresponding char\r");
	    return(0);
	}
	if ((c = *np) < '`')
	    c -= '@';
	else
	    c -= '`';
	switch (n = scr_mapadd(c, cp)){
	  case 0:
	    break;
	  case SCR_MAPERR_CODE:
	    scr_puts("Invalid code: code must be 'A' to 'Z'\r");
	    break;
	  case SCR_MAPERR_FUNC:
	    scr_puts("Invalid func.\r");
	    break;
	  default:
	    scr_puts("unknown result\r");
	}
	return(0);
    } else if (strcmp(np, "keyclear") == 0){
	if ((np = strtok(NULL, " ")) == NULL){
	    scr_mapclear();
	    scr_puts("Keymap cleared.\r");
	} else {
	    if ((c = *np) < '`')
		c -= '@';
	    else
		c -= '`';
	    switch(n = scr_mapadd(c, NULL)){
	      case 0:
		break;
	      case SCR_MAPERR_CODE:
		scr_puts("Invalid code: code must be 'A' to 'Z'\r");
		break;
	      case SCR_MAPERR_FUNC:
		scr_puts("Invalid func.\r");
		break;
	      default:
		scr_puts("unknown result\r");
	    }
	}
    } else if (c == '?'){
	scr_puts("ret                      .. return to SWORD\r"
		 "cd [directory]           .. chdir\r"
		 "mount [drive [filename]] .. mount/umount disk image file\r"
		 "keymap [function char]   .. map function to control code\r"
		 "keyclear [char]          .. clear current keymap\r"
		 "?                        .. display this help\r"
		 /* "! .. shell command\r" */
		 );
	return(0);
    }
#if 0				/* Hmm... screen is raw mode... */
    else if (c == '!'){
	while(*(++p) == ' ')
	    ;
	if (*p == '\0'){	/* no argument */
	    if ((p = getenv("SHELL")) == NULL)
		p = "/bin/sh";
	}
	system(p);
	return(0);
    }
#endif
    else
	scr_puts("Unknown command.\r");
    return(0);		/* not quit command */
}

/*
   SWORD shell
*/
void
ccp(void){
    char	buf[CCP_LINLIM];

    for(;;){
	scr_puts("\r$ ");		/* prompt */
	(void) scr_getl(buf);
	if (ccpline(buf, CCP_MODE_NONE))
	    return;
    }
}

void
readrc(void){
    FILE	*fp;
    char	*rcfilename;
    char	buf[SOS_UNIX_BUFSIZ];
    int		len;

    if ((fp = fopen(RCFILE, "r")) == NULL){
	if (getenv("HOME") == NULL ||
	    (rcfilename = strdup(getenv("HOME"))) == NULL)
	    return;
	if ((rcfilename = realloc(rcfilename, strlen(rcfilename) + sizeof(RCFILE) + 1)) == NULL)
	    return;

	strcat(rcfilename, "/" RCFILE);
	if ((fp = fopen(rcfilename, "r")) == NULL)
	    return;
    }

    while(!feof(fp)){
	if (fgets(buf, sizeof(buf), fp) == NULL)
	    break;
	if ((len = strlen(buf)) > 1)
	    buf[len-1] = 0;
	ccpline(buf, CCP_MODE_INIT);
    }
}

/*
   load file to Z80 memory

   if addr < 0, load to address recorded to file
   return 0 if success

   no file conversion
*/
int
fileload(char *name, int addr){
    int		fattr, fdtadr, fexadr, fsize;
    BYTE	*p;
    int		r;

    if (r = dio_ropen(name, &fattr, &fdtadr, &fsize, &fexadr, 0))
	return(r);

    if (addr < 0)
	addr = fdtadr;
    addr &= 0xffff;

    return(dio_rdd(ram + addr, fsize));
}



void
coldboot(void){
    (void) trap_init();

    sp = EM_STKAD;
}

int
z80loop(void){
    int	r;
    WORD	xpc;

    for(;;){
	switch((r = trap((int) ram[simz80(pc)]))){
	  case TRAP_NEXT:
	    pc++;
	    break;
	  case TRAP_HOLD:
	    break;
	  case TRAP_COLD:
	    xpc = pc;		/* backup PC */
	    coldboot();
	    pc = xpc;
	    break;
	  case TRAP_MON:
	    ccp();
	    pc++;
	    break;
	  case TRAP_QUIT:
	    emu_quit();
	    /* not reach */
	  default:
	    fprintf(stderr,"SOS: unkown trap result %d\n", r);
	    exit(1);
	}
    }
    /* not reach */
    return(TRAP_QUIT);		/* against warning */
}

/** Try to load the sword dos file from a user directory or the system data directory.
    @retval 0 success
    @retval -1 Can not load the sword dos module.
 */
static int
setup_dos_file(void){
	int rc;
	char pathname[SOS_UNIX_PATH_MAX];

	rc = fileload(dosfile, -1); /* load from current directory. */

	if ( rc != 0 ) { /* File not found on current directory. */

		/* load from data directory. */
		snprintf(pathname, SOS_UNIX_PATH_MAX,
		    "%s/%s", DATADIR, dosfile);
		scr_puts(pathname);
		rc = fileload(pathname, -1);
		if ( rc != 0 )
			goto err_out;

	}

	return 0; /* success */
err_out:
	scr_puts("load: failed to load dos module <");
	scr_puts(dosfile);
	scr_puts(">\r");
	(void) scr_finish();

	exit(1);
}

int
main(int argc, char **argv)
{
    int	                c;
    int                rc;
    int	    loadaddr = -1;
    int     jumpaddr = -1;
    char *loadfile = NULL;

    /* default */
    dosfile = DOSFILE;

    while ((c = getopt(argc, argv, "l:a:j:d:c")) != EOF){
	switch (c) {
	  case 'l':
	    loadfile = optarg;
	    break;
	  case 'a':
	    loadaddr = (int) strtol(optarg, NULL, 16);
	    break;
	  case 'j':
	    jumpaddr = (int) strtol(optarg, NULL, 16);
	    break;
	  case 'd':
	    dosfile = optarg;
	    break;
	  case 'c':
	    scr_caps(1);	/* enable caps lock */
	    break;
	  case '?':
	    fprintf(stderr,"%s: [-d dosfile] [-a addr] [-l file] [-j addr] [-c]\n", argv[0]);
	    return(1);
	}
    }

    /* initialize screen */
    if (scr_initx())
	return(1);
    /* define default keymap */
    setdefaultkeymap();

    scr_puts("S-OS Emulator version " VERSION
	 ", Copyright 1996,1997 Takamichi Tateoka.\r"
	 "Derived from CP/M Emulator yaze, Copyright 1995 Frank D. Cringle.\r"
	 "This program comes with ABSOLUTELY NO WARRANTY; for details\r"
	 "see the file \"COPYING\" in the distribution directory.\r\r");

    readrc();

    setup_dos_file();

    if (loadfile != NULL){
	if (fileload(loadfile, loadaddr)){
	    scr_puts("load: failed to load <");
	    scr_puts(loadfile);
	    scr_puts(">\r");
	    (void)scr_finish();
	    return(1);
	}
    }

    coldboot();

    if (jumpaddr > 0){
	z80_push(SOS_BOOT);		/* quit address */
	pc = jumpaddr;
    } else {
	pc = SOS_COLD;
    }
    z80loop();

    emu_quit();
    /* not reach */

    return(0);	/* against compiler's warning */
}
