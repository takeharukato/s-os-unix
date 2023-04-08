/*
   SWORD Emurator  Disk I/O module

   tate@spa.is.uec.ac.jp
*/
#include <stdio.h>

#ifndef	_DIO_H_
#define	_DIO_H_

#include "sim-type.h"
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

#define SOS_TAPE_COMMON_IDX   (0)  /* Common MZ format tape */
#define SOS_TAPE_MONITOR_IDX  (1)  /* Monitor specific format tape */
#define SOS_TAPE_QD_IDX       (2)  /* Quick disk */
#define SOS_TAPE_NR           (3)  /* The number of tape devices */

/** Determine whether device is a disk.
    @param[in] _dsk drive letter to be checked.
 */
#define sos_device_is_disk(_dsk)			\
	( ( SOS_DL_RESV_MAX >= (_dsk) ) && ( (_dsk) >= SOS_DL_DRIVE_A ) )

/** Determine whether device is standard disks.
    @param[in] _dsk drive letter to be checked.
 */
#define sos_device_is_standard_disk(_dsk)		\
	( ( SOS_DL_DRIVE_D >= (_dsk) ) && ( (_dsk) >= SOS_DL_DRIVE_A ) )

/** Determine whether device is a tape.
    @param[in] _dsk drive letter to be checked.
 */
#define sos_device_is_tape(_dsk)					\
	( ( (_dsk) == SOS_DL_COM_CMT ) ||				\
	    ( (_dsk) == SOS_DL_MON_CMT ) ||				\
	    ( (_dsk) == SOS_DL_QD ) )

/** convert a device letter to an index of sos_tape_device_info array.
    @param[in] _dsk drive letter
    @return an index of sos_tape_device_info array
 */
#define sos_tape_devindex(_dsk) 			\
	( (_dsk) == SOS_DL_COM_CMT ? SOS_TAPE_COMMON_IDX :		\
	    ( (_dsk) == SOS_DL_MON_CMT ? SOS_TAPE_MONITOR_IDX : SOS_TAPE_QD_IDX ) )

/** convert an index of sos_tape_device_info array to a device letter.
    @param[in] index of sos_tape_device_info array
    @return drive letter
 */
#define sos_tape_drive_letter(_idx) 				\
	( (_idx) == SOS_TAPE_COMMON_IDX ? SOS_DL_COM_CMT :	\
	    ( (_idx) == SOS_TAPE_MONITOR_IDX ? SOS_DL_MON_CMT : SOS_DL_QD ) )

/* Tape device emulation */
typedef struct _sos_tape_device_info{
	BYTE    dsk;  /**< device letter */
	BYTE  dirno;  /**< #DIRNO of the device */
	BYTE retpoi; /**< RETPOI of the device */
}sos_tape_device_info;
#endif
