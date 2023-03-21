/*
   SWORD Emurator  S-OS system call handler
*/

#ifndef	_TRAP_H_
#define	_TRAP_H_

#include "sim-type.h"

/*
   entry points
*/
int trap(int func);
int trap_init(void);

BYTE trap_get_byte(WORD _addr);
WORD trap_get_word(WORD _addr);
void trap_put_byte(WORD _addr, BYTE _val);
void trap_put_word(WORD _addr, WORD _val);
int trap_write_workarea_without_sync(WORD _addr, BYTE _val);
void trap_change_tape(char _dev);
/*
   return values from TRAP routine
*/
#define	TRAP_NEXT	(0)		/* go next instruction */
#define	TRAP_HOLD	(1)		/* re-interpret current PC */
#define	TRAP_COLD	(2)		/* restart SWORD Emurator */
#define	TRAP_MON	(3)		/* jump to monitor */
#define	TRAP_QUIT	(255)		/* exit SWORD */

#endif
