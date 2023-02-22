/*
   SWORD Emurator main header
*/

#ifndef	_SOS_H_
#define	_SOS_H_

/*
   S-OS IOCS call in Z80 memory
   (only a part)
*/
#define	SOS_COLD	(0x1ffd)
#define	SOS_HOT		(0x1ffa)
#define	SOS_BOOT	(0x2036)


/*
   S-OS work area in Z80 memory
*/
#define	SOS_USR (0x1f7e)
#define	SOS_DVSW (0x1f7d)
#define	SOS_LPSW (0x1f7c)
#define	SOS_PRCNT (0x1f7a)
#define	SOS_XYADR (0x1f78)
#define	SOS_KBFAD (0x1f76)
#define	SOS_IBFAD (0x1f74)
#define	SOS_SIZE (0x1f72)
#define	SOS_DTADR (0x1f70)
#define	SOS_EXADR (0x1f6e)
#define	SOS_STKAD (0x1f6c)
#define	SOS_MEMEX (0x1f6a)
#define	SOS_WKSIZ (0x1f68)
#define	SOS_DIRNO (0x1f67)
#define	SOS_MXTRK (0x1f66)
#define	SOS_DTBUF (0x1f64)
#define	SOS_FATBF (0x1f62)
#define	SOS_DIRPS (0x1f60)
#define	SOS_FATPOS (0x1f5e)
#define	SOS_DSK (0x1f5d)
#define	SOS_WIDTH (0x1f5c)
#define	SOS_MAXLIN (0x1f5b)

#define	SOS_FTYPE	(0x291f)
#define	SOS_DFDV	(0x2920)

#define	SOS_UNITNO	(0x2b06)

#define	SOS_FNAMELEN	(16)
#define SOS_FNAMENAMELEN	(13)
#define	SOS_FNAMEEXTLEN		(3)

#define	SOS_MAXIMAGEDRIVES	(4)

/*
   Emulator setting
*/
#define	EM_XYADR	(0x1171)
#define	EM_KBFAD	(0x11a3)
#define	EM_IBFAD	(0x10f0)
#define	EM_SIZE		(0x1102)
#define	EM_DTADR	(0x1104)
#define	EM_EXADR	(0x1106)
#define	EM_STKAD	(0x10f0)
#define	EM_MEMAX	(0xffff)
#define	EM_WKSIZ	(0xffff)
#define	EM_MXTRK	(0x50)
#define	EM_DTBUF	(0x2f00)
#define	EM_FATBF	(0x2e00)
#define	EM_DIRPS	(0x10)
#define	EM_FATPOS	(0x0e)
#define	EM_WIDTH	(0x50)
#define	EM_MAXLN	(25)

#define	EM_DFDV		('Q')

#define	EM_VER		(0x1620)	/* XXX: SWORD version */

#endif
