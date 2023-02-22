/*
   SWORD Emurator  Disk I/O module

   tate@spa.is.uec.ac.jp
*/
#include <stdio.h>

#ifndef	_DIO_H_
#define	_DIO_H_

#include "sos.h"

/* raw I/O */
int dio_dread(unsigned char *buf, int diskno, int recno, int numrec);
int dio_dwrite(unsigned char *buf, int diskno, int recno, int numrec);
void dio_diclose(int diskno);

/* file I/O */
int dio_wopen(char *name, int attr, int dtadr, int size, int exadr);
int dio_ropen(char *name, int *attr, int *dtadr, int *size, int *exadr, int conv);
int dio_dopen(char *namebuf, int *attr, int *dtadr, int *size, int *exadr, int dirno);
int dio_wdd(unsigned char *buf, int len);
int dio_rdd(unsigned char *buf, int len);

/* disk image file name */
extern char	*dio_disk[SOS_MAXIMAGEDRIVES];

#endif
