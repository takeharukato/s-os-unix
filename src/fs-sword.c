/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator sword file system module                           */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include <string.h>
#include <errno.h>

#include "config.h"

#include "freestanding.h"
#include "bswapmac.h"
#include "sim-type.h"
#include "misc.h"
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
int
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
int
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

/** Search a file in the directory entry on the disk.
    @param[in] ch    The drive letter
    @param[in] dirps The record number of the directory entry
    @param[in] swd_fname The file name in SWORD(NOT C String)
    @param[out] fib  The destination address of the file information block
 */
int
search_dent_sword(sos_devltr ch, BYTE dirps, const BYTE *swd_fname,
    struct _storage_fib *fib){
	int                     rc;
	int                      i;
	BYTE                   rec;
	BYTE                 dirno;
	BYTE                  attr;
	BYTE                *fname;
	WORD                 rdcnt;
	BYTE                  *res;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	for(rec = dirps, dirno = 0; ; ++rec) {

		/*
		 * Read a directory entry
		 */
		rc = storage_record_read(ch, &buf[0], rec, 1, &rdcnt);
		if ( ( rc != 0 ) || ( rdcnt != 1 ) )
			goto error_out;  /* I/O Error */

		for(i = 0, dent = &buf[0]; SOS_DENTRIES_PER_REC > i ;
		    ++i, ++dirno, dent += SOS_DENTRY_SIZE ) {

			attr = *( dent + SOS_FIB_OFF_ATTR );
			fname = ( dent + SOS_FIB_OFF_FNAME );

			if ( attr == SOS_FATTR_FREE )
				continue; /* Free entry */

			if ( attr == SOS_FATTR_EODENT ) {

				rc = SOS_ERROR_NOENT; /* File not found */
				goto error_out;
			}

			if ( memcmp(fname, swd_fname, SOS_FNAME_NAMELEN) == 0 )
				goto found;
		}
	}

found:
	/*
	 * Fill the file information block
	 */
	STORAGE_FILL_FIB(fib,ch,rec,dirno,dent);

	return 0;

error_out:
	return rc;
}
