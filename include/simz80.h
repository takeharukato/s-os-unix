/* Header file for the instruction set simulator.
   Copyright (C) 1995  Frank D. Cringle.

This file is part of yaze - yet another Z80 emulator.

Yaze is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/*
  Modfidied for S-OS emulator
    1996/10/6	Takamichi Tateoka <tate@spa.is.uec.ac.jp>
*/

#ifndef	_SIMZ80_H_
#define	_SIMZ80_H_

#include "sim-type.h"
#include "trap.h"

/* two sets of accumulator / flags */
extern WORD af[2];
extern int af_sel;

/* two sets of 16-bit registers */
extern struct ddregs {
	WORD bc;
	WORD de;
	WORD hl;
} regs[2];
extern int regs_sel;

extern WORD ir;
extern WORD ix;
extern WORD iy;
extern WORD sp;
extern WORD pc;
extern WORD IFF;

extern BYTE ram[64*1024];

#ifdef DEBUG
extern volatile int stopsim;
#endif

extern FASTWORK simz80(FASTREG PC);

#define FLAG_C	1
#define FLAG_N	2
#define FLAG_P	4
#define FLAG_H	16
#define FLAG_Z	64
#define FLAG_S	128

#define SETFLAG(f,c)	AF = (c) ? AF | FLAG_ ## f : AF & ~FLAG_ ## f
#define TSTFLAG(f)	((AF & FLAG_ ## f) != 0)

#define ldig(x)		((x) & 0xf)
#define hdig(x)		(((x)>>4)&0xf)
#define lreg(x)		((x)&0xff)
#define hreg(x)		(((x)>>8)&0xff)

#define Setlreg(x, v)	x = (((x)&0xff00) | ((v)&0xff))
#define Sethreg(x, v)	x = (((x)&0xff) | (((v)&0xff) << 8))

#define RAM(a)		ram[ (a) & 0xffff ]
#define GetBYTE_INTERNAL(a)	( RAM( (a) ) )
#define GetWORD_INTERNAL(a)	( RAM( (a) ) | (RAM( (a) + 1 ) << 8) )
#define PutBYTE_INTERNAL(a, v)	do{		\
		RAM((a)) = (v);			\
	}while(0)
#define PutWORD_INTERNAL(a, v)	do {				       \
		RAM( (a) ) = (BYTE)(v);				       \
		RAM( (a) + 1 ) = (v) >> 8;			       \
	} while (0)

#define GetBYTE(a)     trap_get_byte(a)
#define PutBYTE(a, v)  trap_put_byte(a,v)
#define GetWORD(a)     trap_get_word(a)
#define PutWORD(a, v)  trap_put_word(a,v)
/* Define these as macros or functions if you really want to simulate I/O */
#define Input(port)	0
#define Output(port, value)

#endif
