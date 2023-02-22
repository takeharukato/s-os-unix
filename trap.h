/*
   SWORD Emurator  S-OS system call handler
*/

#ifndef	_TRAP_H_
#define	_TRAP_H_

/*
   entry points
*/
int trap(int func);
int trap_init(void);

/*
   return values from TRAP routine
*/
#define	TRAP_NEXT	(0)		/* go next instruction */
#define	TRAP_HOLD	(1)		/* re-interpret current PC */
#define	TRAP_COLD	(2)		/* restart SWORD Emurator */
#define	TRAP_MON	(3)		/* jump to monitor */
#define	TRAP_QUIT	(255)		/* exit SWORD */

#endif
