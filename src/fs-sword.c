/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator sword file system module                           */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include "config.h"

#include "freestanding.h"

#include "storage.h"

/** Read file allocation table (FAT)
    @param[in]  ch  The device letter of the device
    @param[in]  rec The record number of the FAT
    @param[out] fatbuf Memory buffer for the FAT
    @retval      0  Success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
    @retval ENOMEM  Out of memory.
 */
static int
read_fat_sword(sos_devltr ch, BYTE rec, void *fatbuf){
	int     rc;
	WORD rdcnt;

	rc = storage_record_read(ch, fatbuf,
		    rec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &rdcnt);

	if ( rc != 0 )
		return rc;

	if ( rdcnt != SOS_FAT_SIZE/SOS_RECORD_SIZE )
		return SOS_ERROR_IO;

	return 0;
}

/** Write file allocation table (FAT)
    @param[in]  ch  The device letter of the device
    @param[in]  rec The record number of the FAT
    @param[in] fatbuf Memory buffer for the FAT
    @retval      0  Success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
    @retval ENOMEM  Out of memory.
 */
static int
write_fat_sword(sos_devltr ch, BYTE rec, const void *fatbuf){
	int     rc;
	WORD wrcnt;

	rc = storage_record_write(ch, fatbuf,
		    rec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &wrcnt);

	if ( rc != 0 )
		return rc;

	if ( wrcnt != SOS_FAT_SIZE/SOS_RECORD_SIZE )
		return SOS_ERROR_IO;

	return 0;
}

int
search_dent_sword(sos_devltr ch, const char *fname){
}
