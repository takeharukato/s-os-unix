/*
   SWORD  trap handler
   $Id: trap.c,v 1.8 1999/02/17 04:24:35 tate Exp tate $

   tate@spa.is.uec.ac.jp
*/

#include "config.h"

#include <stdio.h>
/*
#include <unistd.h>
*/
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "simz80.h"
#include "trap.h"
#include "sos.h"
#include "screen.h"
#include "util.h"
#include "dio.h"

/*
   trap functions
*/
int sos_cold(void);
int sos_ver(void);
int sos_print(void);
int sos_prints(void);
int sos_ltnl(void);
int sos_nl(void);
int sos_msg(void);
int sos_msx(void);
int sos_mprint(void);
int sos_tab(void);
int sos_lprint(void);
int sos_lpton(void);
int sos_lptof(void);
int sos_getl(void);
int sos_getky(void);
int sos_brkey(void);
int sos_inkey(void);
int sos_pause(void);
int sos_bell(void);
int sos_prthx(void);
int sos_prthl(void);
int sos_asc(void);
int sos_hex(void);
int sos_2hex(void);
int sos_hlhex(void);
int sos_file(void);
int sos_fsame(void);
int sos_fprnt(void);
int sos_poke(void);
int sos_pokea(void);
int sos_peek(void);
int sos_peeka(void);
int sos_mon(void);
int sos_hl(void);
int sos_getpc(void);
int sos_csr(void);
int sos_scrn(void);
int sos_loc(void);
int sos_flget(void);
int sos_inp(void);
int sos_out(void);
int sos_widch(void);
int sos_dread(void);
int sos_dwrite(void);
int sos_rdi(void);
int sos_tropn(void);
int sos_wri(void);
int sos_twrd(void);
int sos_trdd(void);
int sos_tdir(void);
int sos_parsc(void);
int sos_parcs(void);
int sos_boot(void);

/*
   trap function table
*/
struct functbl {
    /* pointer of function of the trap
       if func == NULL, "JP zaddr" is used instead */
    int	(*func)(void);
    /* Z80 address of the trap entry */
    WORD	calladdr;
    /* Z80 address of proxy entry */
    WORD	zaddr;
} sos_funcs[] = {
  { sos_cold, 0x1ffd , 0},
  { NULL, 0x1ffa , 0x2100},		/* #hot */
  { sos_ver, 0x1ff7 , 0},
  { sos_print, 0x1ff4 , 0},
  { sos_prints, 0x1ff1 , 0},
  { sos_ltnl, 0x1fee , 0},
  { sos_nl, 0x1feb , 0},
  { sos_msg, 0x1fe8, 0},
  { sos_msx, 0x1fe5, 0},
  { sos_mprint, 0x1fe2, 0},
  { sos_tab, 0x1fdf, 0},
  { sos_lprint, 0x1fdc, 0},
  { sos_lpton, 0x1fd9, 0},
  { sos_lptof, 0x1fd6, 0},
  { sos_getl, 0x1fd3, 0},
  { sos_getky, 0x1fd0, 0},
  { sos_brkey, 0x1fcd, 0},
  { sos_inkey, 0x1fca, 0},
  { sos_pause, 0x1fc7, 0},
  { sos_bell, 0x1fc4, 0},
  { sos_prthx, 0x1fc1, 0},
  { sos_prthl, 0x1fbe, 0},
  { sos_asc, 0x1fbb, 0},
  { sos_hex, 0x1fb8, 0},
  { sos_2hex, 0x1fb5, 0},
  { sos_hlhex, 0x1fb2, 0},
  { NULL, 0x1faf, 0x22b3},		/* #wopen */
  { NULL, 0x1fac, 0x232d},		/* #wrd */
  { NULL, 0x1fa9, 0x237c},		/* #fcb */
  { NULL, 0x1fa6, 0x234f},		/* #rdd */
  { sos_file, 0x1fa3, 0},
  { sos_fsame, 0x1fa0, 0},
  { sos_fprnt, 0x1f9d, 0},
  { sos_poke, 0x1f9a, 0},
  { sos_pokea, 0x1f97, 0},
  { sos_peek, 0x1f94, 0},
  { sos_peeka, 0x1f91, 0},
  { sos_mon, 0x1f8e, 0},
  { sos_hl, 0x1f81, 0},
  { sos_getpc, 0x1f80, 0},
  { NULL, 0x2000, 0x2544},		/* #drdsb */
  { NULL, 0x2003, 0x255a},		/* #dwtsb */
  { NULL, 0x2006, 0x2419},		/* #dir */
  { NULL, 0x2009, 0x22fa},		/* #ropen */
  { NULL, 0x200c, 0x2508},		/* #set */
  { NULL, 0x200f, 0x2526},		/* #reset */
  { NULL, 0x2012, 0x24ac},		/* #name */
  { NULL, 0x2015, 0x2477},		/* #kill */
  { sos_csr, 0x2018, 0},
  { sos_scrn, 0x201b, 0},
  { sos_loc, 0x201e, 0},
  { sos_flget, 0x2021, 0},
  { NULL, 0x2024, 0x25ad},		/* #rdvsw */
  { NULL, 0x2027, 0x25c9},		/* #sdvsw */
  { sos_inp, 0x202a, 0},
  { sos_out, 0x202d, 0},
  { sos_widch, 0x2030, 0},
  { NULL, 0x2033, 0x286c},		/* #error */
  { sos_boot, 0x2036, 0},
  /* disk I/O */
  { sos_dread, 0x2b00, 0},
  { sos_dwrite, 0x2b03, 0},
  /* sword dos module internal hook */
  { sos_rdi, 0x2900, 0},
  { sos_tropn, 0x2903, 0},
  { sos_wri, 0x2906, 0},
  { sos_twrd, 0x2909, 0},
  { sos_trdd, 0x290c, 0},
  { sos_tdir, 0x290f, 0},
  { NULL, 0x2912, 0x27e3},		/* p#fnam */
  { NULL, 0x2915, 0x2851},		/* devchk */
  { NULL, 0x2918, 0x2863},		/* tpchk */
  { sos_parsc, 0x292a, 0},
  { sos_parcs, 0x293f, 0},
};

/* total number of traps */
#define	trap_nfunc	(sizeof sos_funcs / sizeof(struct functbl))

/* file attributes */
char *trap_attr[] = {
	"Nul",	/* 0 */
	"Bin",	/* 1 */
	"Bas",	/* 2 */
	"???",
	"Asc",	/* 4 */
	"???",
	"???",
	"???",
	"Dir",	/* 8 = 0x80 */
};
#define trap_nattr 	(sizeof trap_attr / sizeof(char *))

/*
   useful defines
*/
#define	Z80_HALT	(0x76)
#define	Z80_RET		(0xc9)
#define	Z80_JP		(0xc3)

#define	Z80_A		(hreg(af[af_sel]))
#define	Z80_B		(hreg(regs[regs_sel].bc))
#define	Z80_C		(lreg(regs[regs_sel].bc))
#define	Z80_D		(hreg(regs[regs_sel].de))
#define	Z80_E		(lreg(regs[regs_sel].de))
#define	Z80_H		(hreg(regs[regs_sel].hl))
#define	Z80_L		(lreg(regs[regs_sel].hl))
#define	Z80_AF		(af[af_sel])
#define	Z80_BC		(regs[regs_sel].bc)
#define	Z80_DE		(regs[regs_sel].de)
#define	Z80_HL		(regs[regs_sel].hl)
#define	Z80_SP		(sp)
#define	Z80_PC		(pc)

/*
   trick
*/
#undef	SETFLAG
#undef	TSTFLAG
#define SETFLAG(f,c)	Z80_AF = (c) ? Z80_AF | FLAG_ ## f : Z80_AF & ~FLAG_ ## f
#define TSTFLAG(f)	((Z80_AF & FLAG_ ## f) != 0)

/*
   variables
*/
BYTE	wkram[EM_WKSIZ+1];	/* S-OS special work */

/*
   initialize trap handler & SWORD memory
*/
int
trap_init(void){
    WORD	addr;
    int		funcnum;

    /* create SWORD system call table */
    funcnum = 0;
    for(funcnum=0; funcnum < trap_nfunc; funcnum++){
	addr = sos_funcs[funcnum].calladdr;
	if (sos_funcs[funcnum].func != NULL){
	    /* install trap entry */
	    PutBYTE(addr, Z80_HALT);
	    PutBYTE(addr+1, (BYTE) funcnum);
	    PutBYTE(addr+2, Z80_RET);
	} else {
	    /* jump to another Z80 code */
	    PutBYTE(addr, Z80_JP);
	    PutWORD(addr+1, sos_funcs[funcnum].zaddr);
	}
    }
    /* fix some tricks */
    PutBYTE(0x1f80, 0xe1);
    PutBYTE(0x1f81, 0xe9);

    PutWORD(0x1f7e, 0x1ffa);	/* #USR */
    PutBYTE(0x1f7d, 0);		/* #DVSW */
    PutBYTE(0x1f7c, 0);		/* #LPSW */
    PutWORD(0x1f7a, 0);		/* #PRCNT */
    PutWORD(0x1f78, EM_XYADR);	/* #XYADR */
    PutWORD(0x1f76, EM_KBFAD);	/* #KBFAD */
    PutWORD(0x1f74, EM_IBFAD);	/* #IBFAD */
    PutWORD(0x1f72, 0);		/* #SIZE */
    PutWORD(0x1f70, 0);		/* #DTADR */
    PutWORD(0x1f6e, 0);		/* #EXADR */
    PutWORD(0x1f6c, EM_STKAD);	/* #STKAD */
    PutWORD(0x1f6a, EM_MEMAX);	/* #MEMAX */
    PutWORD(0x1f68, EM_WKSIZ);	/* #WKSIZ */
    PutBYTE(0x1f67, 0);		/* #DIRNO */
    PutBYTE(0x1f66, EM_MXTRK);	/* #MXTRK */
    PutWORD(0x1f64, EM_DTBUF);	/* #DTBUF */
    PutWORD(0x1f62, EM_FATBF);	/* #FATBF */
    PutWORD(0x1f60, EM_DIRPS);	/* #DIRPS */
    PutWORD(0x1f5e, EM_FATPOS);	/* #FATPOS */
    PutBYTE(0x1f5d, EM_DFDV);	/* #DSK */
    PutBYTE(0x1f5c, EM_WIDTH);	/* #WIDTH */
    PutBYTE(0x1f5b, EM_MAXLN);	/* #MAXLIN */

    PutBYTE(SOS_DFDV, EM_DFDV);	/* %DFDV */

    return(0);
}


/*
   SWORD trap handler

   call when HALT on emulation
*/
int
trap(int func){
    int	r;
    char buf[TRAP_BUFSIZ];

    if (func < 0 || trap_nfunc <= func || sos_funcs[func].func == NULL){

	    snprintf(buf, TRAP_BUFSIZ, "\nSOS Emulator: Invalid trap: %d\r",func);
	    scr_puts(buf);
	    return TRAP_COLD;
    } else
	r = (*sos_funcs[func].func)();

    return r;
}

int sos_cold(void){
    Z80_PC = GetWORD(SOS_USR);
    return(TRAP_COLD);
}

int sos_ver(void){
    Z80_HL = EM_VER;
    return(TRAP_NEXT);
}

int sos_print(void){
    scr_putchar(Z80_A);
    return(TRAP_NEXT);
}

int sos_prints(void){
    scr_putchar(' ');
    return(TRAP_NEXT);
}

int sos_ltnl(void){
    scr_ltnl();
    return(TRAP_NEXT);
}

int sos_nl(void){
    scr_nl();
    return(TRAP_NEXT);
}

int sos_msg(void){
    WORD	addr;
    char	c;

    addr = Z80_DE;
    while((c = GetBYTE(addr++)) != 0x0d){
	scr_asyncputchar(c);
    }
    scr_sync();
    return(TRAP_NEXT);
}

int sos_msx(void){
    scr_puts((char *)&(ram[Z80_DE]));
    return(TRAP_NEXT);
}

/*
  NOTE:
   SWORD system call table describes this function breaks AF and DE
   but at least MZ-80K & MZ-2000 implementation saves DE reg. and
   some programs include REDA expect it.
*/
int sos_mprint(void){
    WORD	addr;
    char	c;

    addr = z80_pop();
    while((c = GetBYTE(addr++)) != 0){		/* search return addr */
	scr_asyncputchar(c);
    }
    scr_sync();
    z80_push(addr);
    return(TRAP_NEXT);
}

int sos_tab(void){
    scr_tab(Z80_B);
    return(TRAP_NEXT);
}

int sos_lprint(void){
    /* XX: not support */
    return(TRAP_NEXT);
}

int sos_lpton(void){
    /* XX: not support */
    return(TRAP_NEXT);
}

int sos_lptof(void){
    /* XX: not support */
    return(TRAP_NEXT);
}

int sos_getl(void){
    char	buf[2000];
    int		len;

    len =  scr_getl(buf);
    memcpy(ram + Z80_DE, buf, len+1);	/* copy with last '\0' */
    /* NOTE: some caller (includes DOS module) require filling zero
             onto rest of buffer, to rid a overrun. */
    if (len < EM_WIDTH-1)
	memset(ram + Z80_DE + len + 1, '\0', EM_WIDTH - 1 - len);
    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_getky(void){
    Sethreg(Z80_AF, scr_getky());
    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_brkey(void){
    if (scr_brkey()){
	SETFLAG(Z, 1);
    } else {
	SETFLAG(Z, 0);
    }
    return(TRAP_NEXT);
}

int sos_inkey(void){
    Sethreg(Z80_AF, scr_inkey());
    return(TRAP_NEXT);
}

int sos_pause(void){
    WORD	addr;

    if (scr_pause()){
	/* break */
	addr = z80_pop();
	addr = GetWORD(addr);
	z80_push(addr);
	return(TRAP_HOLD);
    }
    /* space pressed, so skip 2 byte */
    addr = z80_pop();
    addr += 2;
    z80_push(addr);

    return(TRAP_NEXT);
}

int sos_bell(void){
    scr_bell();
    return(TRAP_NEXT);
}

int sos_prthx(void){
    char	buf[3];

    snprintf(buf, 3, "%02X", (int) Z80_A);
    scr_puts(buf);
    return(TRAP_NEXT);
}

int sos_prthl(void){
    char	buf[5];

    snprintf(buf, 5, "%04X", (int) Z80_HL);
    scr_puts(buf);
    return(TRAP_NEXT);
}

int sos_asc(void){
    int	c;

    c = (int)Z80_A & 0x0f;
    if (c < 10){
	Sethreg(Z80_AF, c);
    } else {
	Sethreg(Z80_AF, c + 'A');
    }
    return(TRAP_NEXT);
}

int hexone(int c){
    if ('0' <= c && c <= '9'){
	return(c - '0');
    } else if ('A' <= c && c <= 'F'){
	return(c - 'A' + 10);
    } else if ('a' <= c && c <= 'f'){
	return(c - 'a' + 10);
    }
    return(-1);
}

int sos_hex(void){
    int	r;

    if ((r = hexone((int)Z80_A & 0xff)) < 0){
	SETFLAG(C, 1);
    } else {
	Sethreg(Z80_AF, r);
	SETFLAG(C, 0);
    }
    return(TRAP_NEXT);
}

int sos_2hex(void){
    int	result;
    int	r;
    int	i;

    result = 0;
    for (i=0; i<2; i++){
	if ((r = hexone(GetBYTE(Z80_DE++))) < 0){
	    /* XXX: no define about DE on error */
	    SETFLAG(C, 1);
	    return(TRAP_NEXT);
	}
	result = (result << 4) + r;
    }
    Sethreg(Z80_AF, result);
    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_hlhex(void){
    int	result;
    int	r;
    int	i;

    result = 0;
    for (i=0; i<4; i++){
	if ((r = hexone(GetBYTE(Z80_DE++))) < 0){
	    /* XXX: no define about DE on error */
	    SETFLAG(C, 1);
	    return(TRAP_NEXT);
	}
	result = (result << 4) + r;
    }
    Z80_HL = result;
    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

/*
   trap_fname:

   filename parse function
   input:
     Z80_DE  .. top of buffer
     defdsk  .. default drive
   output:
     Z80_DE  .. point of terminator ('\0' or ':')
     buf     .. space padded filename
     dsk     .. drive name
     return  .. 0 if success, or sword error code
*/
int
trap_fname(unsigned char *buf, unsigned char *dsk, unsigned char defdsk){
    WORD	ri;
    BYTE	c;
    int		len;
    unsigned char	d;

    ri = Z80_DE;

    /* get drive name if exist */
    while(ram[ri] == ' ')		/* space skip */
	ri++;
    if (ram[ri + 1] == ':'){
	if (islower(d = ram[ri]))
	    d = toupper(d);
	if ((d < 'A' || 'L' < d) && d != 'T' && d != 'S' && d != 'Q')
	    return(14);			/* bad data */
	ri += 2;
    } else {
	d = defdsk;
    }
    *dsk = d;

    /* get file name */
    while(ram[ri] == ' ')		/* space skip */
	ri++;
    for (len=0; len<13; len++){
	if ((c = ram[ri]) < ' ' || c == ':' || c == '.')
	    break;
	*buf++ = c;
	ri++;
    }
    for (; len<13; len++)
	*buf++ = ' ';			/* space padding */

    /* skip "." of extension */
    if (ram[ri] == '.')
	ri++;

    /* get extention */
    for (len=0; len<3; len++){
	if ((c = ram[ri]) < ' ' || c == ':')
	    break;
	*buf++ = c;
	ri++;
    }
    for (; len<3; len++)
	*buf++ = ' ';			/* space padding */

    *buf = '\0';
    Z80_DE = ri;
    SETFLAG(C, 0);
    return(0);
}

int sos_file(void){
    WORD	wi;
    BYTE	attr;
    unsigned char	buf[SOS_FNAMELEN + 1];
    unsigned char	dsk;
    int		r;

    wi = EM_IBFAD;

    if ((attr = Z80_A) == (BYTE)0xee){
	attr = 4;	/* XX: unknown reason, came from original code */
    }
    PutBYTE(wi, attr);
    PutBYTE(SOS_FTYPE, attr);
    wi++;

    if (r = trap_fname(buf, &dsk, GetBYTE(SOS_DFDV))){
	Sethreg(Z80_AF, r);
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }
    memcpy(ram + wi, buf, SOS_FNAMELEN);
    PutBYTE(SOS_DSK, dsk);

    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_fsame(void){
    unsigned char	buf[SOS_FNAMELEN + 1];
    unsigned char	dsk;
    int	r;

    /* check attribute */
    if (GetBYTE(EM_IBFAD) != Z80_A){
	SETFLAG(Z, 0);
	return(TRAP_NEXT);
    }

    /* parse file name */
    if (r = trap_fname(buf, &dsk, GetBYTE(SOS_DSK))){
	Sethreg(Z80_AF, r);
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }

    /* compare them */
    if (memcmp(buf, ram + EM_IBFAD + 1, SOS_FNAMELEN) == 0){
	SETFLAG(Z, 1);
    } else {
	SETFLAG(Z, 0);
    }
    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_fprnt(void){
    WORD	namep;
    char	buf[SOS_FNAMELEN+2], *p;
    int		i;
    int	c;

    namep = EM_IBFAD + 1;
    /* copy file name */
    p = buf;
    for (i=0; i<SOS_FNAMENAMELEN; i++){
	if ((c = GetBYTE(namep)) < ' '){
	    *p++ = ' ';
	    continue;
	}
	if (c == '.'){
	    c = ' ';
	}
	*p++ = c;
	namep++;
    }
    *p++ = '.';
    /* copy extension */
    for (i=0; i<SOS_FNAMEEXTLEN; i++){
	if ((c = GetBYTE(namep)) < ' '){
	    *p++ = ' ';
	    continue;
	}
	*p++ = c;
	namep++;
    }
    /* print it */
    *p = '\0';
    scr_puts(buf);

    /* check space key */
    scr_pause();

    return(TRAP_NEXT);
}

int sos_poke(void){
    wkram[Z80_HL] = Z80_A;
    return(TRAP_NEXT);
}

int sos_pokea(void){
    WORD	from;
    int		offset;
    int		len;

    from = Z80_HL;
    offset = (int) Z80_DE;
    len = (int) Z80_BC;

    if (offset + len > EM_WKSIZ){
	len = EM_WKSIZ - offset;		/* overflow check */
    }
    memcpy(wkram + offset, ram + from, len);
    SETFLAG(C, 0);	/* S-OS ref. man. p.120 */
    return(TRAP_NEXT);
}

int sos_peek(void){
    Sethreg(Z80_AF, wkram[Z80_HL]);
    return(TRAP_NEXT);
}

int sos_peeka(void){
    int		offset;
    WORD	target;
    int		len;

    offset = (int) Z80_HL;
    target = Z80_DE;
    len = (int) Z80_BC;

    if (offset + len > EM_WKSIZ){
	len = EM_WKSIZ - offset;		/* overflow check */
    }
    memcpy(ram + target, wkram + offset, len);

    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_mon(void){
    return(TRAP_MON);		/* quit to monitor */
}

int sos_hl(void){
    /* never call */
    return(TRAP_NEXT);
}

int sos_getpc(void){
    /* never call */
    return(TRAP_NEXT);
}

int sos_csr(void){
    int	x,y;

    scr_csr(&y, &x);
    Sethreg(Z80_HL, y);
    Setlreg(Z80_HL, x);
    return(TRAP_NEXT);
}

int sos_scrn(void){
    int	c;

    c = scr_scrn(Z80_H, Z80_L);
    Sethreg(Z80_AF, c);
    return(TRAP_NEXT);
}

int sos_loc(void){
    scr_loc((int) Z80_H, (int) Z80_L);

    /* #LOC shuld check validation of arguments and return
       Carry Flag for error. (S-OS ref. man. p.118)
       Currently not supported and only return with no carry */
    SETFLAG(C, 0);

    return(TRAP_NEXT);
}

int sos_flget(void){
    int	c;

    c = scr_flget();
    Sethreg(Z80_AF, c);
    return(TRAP_NEXT);
}

int sos_inp(void){
    /* not support */
    return(TRAP_NEXT);
}

int sos_out(void){
    /* not support */
    return(TRAP_NEXT);
}

int sos_widch(void){
    int	w;

    w = (Z80_A <= 40) ? 40 : 80;
    scr_width(w);
    PutBYTE(SOS_WIDTH, w);
    SETFLAG(C, 0);		/* not ducumented, but required */
    return(TRAP_NEXT);
}

/*
   raw disk I/O
*/
int sos_dread(void){
    int	r;

    r = dio_dread(ram + Z80_HL, (int) GetBYTE(SOS_UNITNO),
		     (int) Z80_DE, (int) Z80_A);
    Sethreg(Z80_AF, r);
    SETFLAG(C, r);
    return(TRAP_NEXT);
}

int sos_dwrite(void){
    int	r;

    r = dio_dwrite(ram + Z80_HL, (int) GetBYTE(SOS_UNITNO),
		      (int) Z80_DE, (int) Z80_A);

    Sethreg(Z80_AF, r);
    SETFLAG(C, r);
    return(TRAP_NEXT);
}

/*
   trap for dos module internal hooks
*/
int sos_tropn(void){
    int	len, attr, addr, exaddr;
    int	r;

    if (r = dio_ropen((char *)ram +EM_IBFAD +1, &attr, &addr, &len, &exaddr, 1)){
	Sethreg(Z80_AF, r);
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }

    PutBYTE(EM_IBFAD, attr);
    PutWORD(SOS_DTADR, addr);
    PutWORD(SOS_EXADR, exaddr);
    PutWORD(SOS_SIZE, len);

    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_rdi(void){
    int	len, attr, addr, exaddr;
    int	dirno;
    int	r;

    if (scr_brkey()){
	PutBYTE(SOS_DIRNO, 0);
	Sethreg(Z80_AF, 8);	/* unknown reason, came from orignal code */
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }
    if (scr_getky() == '\r'){
	Sethreg(Z80_AF, 8);	/* unknown reason, came from orignal code */
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }

    if (r = dio_dopen((char *)ram +EM_IBFAD +1, &attr, &addr, &len,
		      &exaddr, GetBYTE(SOS_DIRNO))){
	PutBYTE(SOS_DIRNO, 0);
	Sethreg(Z80_AF, r);
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }

    PutBYTE(EM_IBFAD, attr);
    PutWORD(SOS_DTADR, addr);
    PutWORD(SOS_EXADR, exaddr);
    PutWORD(SOS_SIZE, len);

    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_wri(void){
    int r;
    int	attr;

    if (r = dio_wopen((char *)ram + EM_IBFAD + 1, GetBYTE(EM_IBFAD),
		      GetWORD(SOS_DTADR), GetWORD(SOS_SIZE),
		      GetWORD(SOS_EXADR))){
	Sethreg(Z80_AF, r);
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }

    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_twrd(void){
    int	r;

    if (r = dio_wdd(ram + GetWORD(SOS_DTADR), GetWORD(SOS_SIZE))){
	Sethreg(Z80_AF, r);
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }

    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_trdd(void){
    int	r;

    if (r = dio_rdd(ram + GetWORD(SOS_DTADR), GetWORD(SOS_SIZE))){
	Sethreg(Z80_AF, r);
	SETFLAG(C, 1);
	return(TRAP_NEXT);
    }
    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_tdir(void){
    int	dirno;
    char	name[SOS_FNAMELEN + 1];
    char	ext[SOS_FNAMEEXTLEN + 1];
    int	        len, attr, addr, exaddr;
    char	buf[SOS_DIRFMTLEN + 1];
    char	*type;

    dirno = 0;
    while(dio_dopen(name, &attr, &addr, &len, &exaddr, dirno) == 0){
	name[SOS_FNAMELEN] = '\0';	/* dopen will not terminate */
	strcpy(ext, name+SOS_FNAMENAMELEN);
	name[SOS_FNAMENAMELEN] = '\0';	/* terminate name part */
	if (attr < trap_nattr)
	    type = trap_attr[attr];
	else
	    type = "???";
	snprintf(buf, SOS_DIRFMTLEN + 1, "%s  Q:%s.%s:%04X:%04X:%04X\r",
		type, name, ext, addr & 0xffff, (addr+len-1) & 0xffff, exaddr& 0xffff);
	scr_puts(buf);
	dirno++;
    }

    SETFLAG(C, 0);
    return(TRAP_NEXT);
}

int sos_parsc(void){
    PutWORD(SOS_SIZE, GetWORD(EM_SIZE));
    PutWORD(SOS_DTADR, GetWORD(EM_DTADR));
    PutWORD(SOS_EXADR, GetWORD(EM_EXADR));
    return(TRAP_NEXT);
}

int sos_parcs(void){
    PutWORD(EM_SIZE, GetWORD(SOS_SIZE));
    PutWORD(EM_DTADR, GetWORD(SOS_DTADR));
    PutWORD(EM_EXADR, GetWORD(SOS_EXADR));
    return(TRAP_NEXT);
}

int sos_boot(void){
    return(TRAP_QUIT);		/* quit emulator */
}
