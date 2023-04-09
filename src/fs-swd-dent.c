/* -*- mode: C; coding:utf-8 -*- */
/*************************************************************************/
/*  SWORD Emulator sword file system module - directory entry operations */
/*                                                                       */
/*  Copyright 2023 Takeharu KATO                                         */
/*                                                                       */
/*************************************************************************/

#include <string.h>
#include <errno.h>

#include "config.h"

#include "freestanding.h"
#include "sim-type.h"
#include "misc.h"
#include "compat.h"
#include "storage.h"
#include "sos.h"
#include "fs-vfs.h"
#include "fs-sword.h"

/** Read the directory entry by #DIRNO
    @param[in]   ch        The drive letter
    @param[in]   dirno     The #DIRNO number of the directory entry to read.
    @param[out]  recp      The address to store the record number of the directory entry.
    @param[out]  dentp     The address to store the directory entry.
    @param[in]   bufsiz    The size of the buffer pointed by DENTP.
    @retval     0               Success
    @retval     SOS_ERROR_IO    I/O Error
    @retval     SOS_ERROR_NOENT File not found
 */
static int
search_dent_by_dirno(sos_devltr ch, fs_dirno dirno, fs_rec *recp,
    BYTE *dentp, size_t bufsiz){
	int                      i;
	int                     rc;
	size_t                 cur;
	fs_rec                 rec;
	fs_dirps         dirps_rec;
	fs_sword_attr         attr;
	size_t               rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];


	rc = storage_get_dirps(ch, &dirps_rec);
	if ( rc != 0 )
		goto error_out;

	for(rec = SOS_DIRPS_VAL(dirps_rec), cur = 0; SOS_DENTRY_NR > cur; ++rec) {

		/*
		 * Read each directory entry
		 */

		/* Read a directory entry record */
		rc = storage_record_read(ch, &buf[0], SOS_REC_VAL(rec), 1, &rdcnt);
		if ( rc != 0 )
			goto error_out;  /* Error */

		if ( rdcnt != 1 ) {

			rc = SOS_ERROR_IO;
			goto error_out;  /* I/O Error */
		}

		/*
		 * Search for the directory entry specified by #DIRNO
		 */
		for(i = 0, dent = &buf[0]; SOS_DENTRIES_PER_REC > i ;
		    ++i, ++cur, dent += SOS_DENTRY_SIZE ) {

			attr = *( dent + SOS_FIB_OFF_ATTR );

			if ( attr == SOS_FATTR_FREE )
				continue; /* Free entry */

			if ( attr == SOS_FATTR_EODENT ) {

				rc = SOS_ERROR_NOENT; /* File not found */
				goto error_out;
			}

			if ( cur == SOS_DIRNO_VAL(dirno) ) {

				if ( recp != NULL )
					*recp = rec;

				if ( dentp != NULL )
					memcpy(dentp, dent,
					    SOS_MIN(bufsiz, SOS_DENTRY_SIZE));

				goto found;
			}
		}
	}

	/*
	 * The end of directory entry was not found.
	 */
	return SOS_ERROR_NOENT; /* File not found */

found:
	return  0;

error_out:
	return rc;

}

/** Search a file in the directory entry on the disk with #DIRNO.
    @param[in]  ch    The drive letter.
    @param[in]  dirno The #DIRNO number of the directory entry to read.
    @param[out] fib   The destination address of the file information block.
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fs_swd_search_dent_by_dirno(sos_devltr ch, fs_dirno dirno,
    struct _storage_fib *fib){
	int                      rc;
	fs_rec                  rec;
	BYTE  dent[SOS_DENTRY_SIZE];

	/* Read a directory entry. */
	rc = search_dent_by_dirno(ch, SOS_DIRNO_VAL(dirno),
	    &rec, &dent[0], SOS_DENTRY_SIZE);
	if ( rc != 0 )
		goto error_out; /* File not found */

	/*
	 * Fill the file information block
	 */
	if ( fib != NULL )
		STORAGE_FILL_FIB(fib, ch, SOS_DIRNO_VAL(dirno), &dent[0]);

	return 0;

error_out:
	return rc;
}

/** Search a file in the directory entry on the disk.
    @param[in] ch     The drive letter
    @param[in] swd_name The file name in SWORD(NOT C String)
    @param[out] fib  The destination address of the file information block
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fs_swd_search_dent_by_name(sos_devltr ch, const BYTE *swd_name,
    struct _storage_fib *fib){
	int                         rc;
	fs_rec                     rec;
	fs_dirno                 dirno;
	BYTE     dent[SOS_DENTRY_SIZE];

	for(dirno = 0; SOS_DENTRY_NR > SOS_DIRNO_VAL(dirno); ++dirno) {

		/* Read each directory entry. */
		rc = search_dent_by_dirno(ch, SOS_DIRNO_VAL(dirno),
		    &rec, &dent[0], SOS_DENTRY_SIZE);
		if ( rc != 0 )
			goto error_out; /* File not found */

		if ( memcmp(&dent[0] + SOS_FIB_OFF_FNAME, &swd_name[0],
			SOS_FNAME_NAMELEN) == 0 )
			goto found;  /* Found */
	}

	sos_assert_no_reach();  /* This function never comes here */

found:
	/*
	 * Fill the file information block
	 */
	if ( fib != NULL )
		STORAGE_FILL_FIB(fib, ch, SOS_DIRNO_VAL(dirno), &dent[0]);

	return 0;

error_out:
	return rc;
}

/** Search a free directory entry on the disk.
    @param[in] ch      The drive letter
    @param[out] dirnop The the address to store #DIRNO of the found entry.
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOSPC Free entry not found
 */
int
fs_swd_search_free_dent(sos_devltr ch, fs_dirno *dirnop){
	int                     rc;
	int                      i;
	fs_rec                 rec;
	fs_dirno             dirno;
	fs_dirps         dirps_rec;
	BYTE                  attr;
	size_t               rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	rc = storage_get_dirps(ch, &dirps_rec);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search a free entry
	 */
	for(rec = SOS_DIRPS_VAL(dirps_rec), dirno = 0;
	    SOS_DENTRY_NR > SOS_DIRNO_VAL(dirno); ++rec) {

		/*
		 * Read a directory entry
		 */
		rc = storage_record_read(ch, &buf[0], rec, 1, &rdcnt);
		if ( rc != 0 )
			goto error_out;  /* Error */

		if ( rdcnt != 1 ) {

			rc = SOS_ERROR_IO;
			goto error_out;  /* I/O Error */
		}

		/*
		 * Search a free entry
		 */
		for(i = 0, dent = &buf[0]; SOS_DENTRIES_PER_REC > i ;
		    ++i, ++dirno, dent += SOS_DENTRY_SIZE ) {

			attr = *( dent + SOS_FIB_OFF_ATTR );
			if ( ( attr == SOS_FATTR_FREE ) || ( attr == SOS_FATTR_EODENT ) )
				goto found; /* an entry was found */
		}
	}

	/*
	 * Reach the end of directory
	 */
	rc = SOS_ERROR_NOSPC; /* Device Full */
	goto error_out;

found:
	/*
	 * Return the #DIRNO of the free entry.
	 */
	if ( dirnop != NULL )
		*dirnop = dirno;

	return 0;

error_out:
	return rc;
}

/** Write the directory entry to the disk.
    @param[in] ch    The drive letter
    @param[in] fib   The address of the file information block
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fs_swd_write_dent(sos_devltr ch, struct _storage_fib *fib){
	int                      rc;
	fs_rec                  rec;
	fs_dirno       dirno_offset;
	fs_dirps          dirps_rec;
	size_t                rwcnt;
	BYTE                  *dent;
	BYTE   buf[SOS_RECORD_SIZE];

	rc = storage_get_dirps(ch, &dirps_rec);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Read directory entry
	 */
	rec = SOS_DIRNO_VAL(fib->fib_dirno) / SOS_DENTRIES_PER_REC
		+ SOS_DIRPS_VAL(dirps_rec);

	rc = storage_record_read(ch, &buf[0], SOS_REC_VAL(rec), 1, &rwcnt);
	if ( rc != 0 )
		goto error_out;  /* Error */

	if ( rwcnt != 1 ) {

		rc = SOS_ERROR_IO;
		goto error_out;  /* I/O Error */
	}

	/* Calculate dirno offset in the record */
	dirno_offset = SOS_DIRNO_VAL(fib->fib_dirno) % SOS_DENTRIES_PER_REC;
	/* refer the directory entry to modify */
	dent = (BYTE *)&buf[0] +  FS_SWD_DIRNO2OFF( SOS_DIRNO_VAL(dirno_offset) );
	STORAGE_FIB2DENT(fib, dent); 	/* Modify the entry */

	/*
	 * Write directory entry
	 */
	rc = storage_record_write(ch, &buf[0], rec, 1, &rwcnt);
	if ( rc != 0 )
		goto error_out;  /* Error */

	if ( rwcnt != 1 ) {

		rc = SOS_ERROR_IO;
		goto error_out;  /* I/O Error */
	}

	return 0;

error_out:
	return rc;
}
