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

/*
 * Macros
 */
#define FS_SWD_GTBLK_RD_FLG   (0)   /* Get block to read */
#define FS_SWD_GTBLK_WR_FLG   (1)   /* Get block to write */
/** Determine the direction of getting block.
    @param[in] _mod  The direction flag
    FS_SWD_GTBLK_RD_FLG Get block to read
    FS_SWD_GTBLK_WR_FLG Get block to write
    @retval TRUE  Get block to write
    @retval FALSE Get block to read
 */
#define FS_SWD_GETBLK_TO_WRITE(_mod) ( (_mod) & FS_SWD_GTBLK_WR_FLG )

/*
 * Foward declarations
 */

/*
 * Internal functions
 */


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

/** Clear block
    @param[in]  ch       The drive letter
    @param[in]  blkno    The block number
 */
static int
clear_block_sword(sos_devltr ch, WORD blkno){
	int                     rc;
	BYTE                   rec;
	WORD                 rwcnt;
	WORD               remains;
	BYTE  buf[SOS_RECORD_SIZE];

	memset(&buf[0], 0x0, SOS_RECORD_SIZE); /* clear data */
	for(rec = SOS_CLS2REC(blkno), remains = SOS_CLUSTER_RECS;
	    remains > 0; ++rec, --remains) {

		/*
		 * clear record
		 */
		rc = storage_record_write(ch, &buf[0], rec, 1, &rwcnt);
		if ( rc != 0 )
			goto error_out;  /* Error */

		if ( rwcnt != 1 ) {

			rc = SOS_ERROR_IO;
			goto error_out;  /* I/O Error */
		}
	}

	return 0;

error_out:
	return rc;
}

/** Allocate new block on the disk.
    @param[in]  ch      The drive letter
    @param[in]  fatrec  The record number of FAT
    @param[out] blknop  The address to store the block number of the new block.
    @retval     0       success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
  */
static int
alloc_newblock_sword(sos_devltr ch, BYTE fatrec, WORD *blknop){
	int                     rc;
	int                      i;
	BYTE                   rec;
	BYTE                 *clsp;
	BYTE  fat[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, fatrec, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search a free cluster.
	 */
	for( i = 0, clsp = &fat[0]; SOS_FAT_NR > i; ++i, clsp = &fat[i]){

		if ( *clsp == SOS_FAT_ENT_FREE ) {

			/* alloc new cluster and use the all records in the culster */
			*clsp = SOS_FAT_ENT_EOF_MASK|0xf;
			goto found;
		}
	}

	rc = SOS_ERROR_NOSPC;  /* Device full */
	goto error_out;

found:
	sos_assert( SOS_IS_END_CLS(*clsp) );
	sos_assert( &fat[i] == clsp );

	clear_block_sword(ch, i);  /* clear new block */

	rc = write_fat_sword(ch, fatrec, &fat[0]);  /* write fat */
	if ( rc != 0 )
		goto error_out;

	if ( blknop != NULL )
		*blknop = i;  /* return the block number */

	return 0;

error_out:
	return rc;
}


/** Release the block in the file.
    @param[in] ch     The drive letter
    @param[in] fatrec The record number of FAT
    @param[in] fib    The file information block of the file contains the block
    @param[in] pos    The file position where the block is placed at
    @retval      0  Success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
 */
static int
release_block_sword(sos_devltr ch, BYTE fatrec, struct _storage_fib *fib, WORD pos) {
	int                     rc;
	int                      i;
	BYTE               remains;
	BYTE              prev_cls;
	WORD                   cls;
	WORD                   rec;
	BYTE  fat[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, fatrec, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;
	/*
	 * Find the block to release
	 */

	/* How many clusters to search */
	remains = SOS_CALC_ALIGN_Z80_WORD(pos % SOS_MAX_FILE_SIZE, SOS_CLUSTER_SIZE);

	/*
	 * Search the cluster to free.
	 */
	for( cls = fib->fib_cls & SOS_FAT_CLSNUM_MASK, prev_cls = cls;
	     remains > 0; prev_cls = cls, cls = fat[cls], --remains) {

		if ( fat[cls] == SOS_FAT_ENT_FREE ) {

			rc = SOS_ERROR_BADFAT;  /* Invalid cluster chain */
			goto error_out;
		}

		if ( SOS_IS_END_CLS(fat[cls]) ) {

			/* return the block not found error  */
			rc = SOS_ERROR_NOENT;
			goto error_out;
		}
	}

	/*
	 * update cluster chain and the size of the file
	 */
	if ( !SOS_IS_END_CLS( fat[cls] ) ) {

		fat[prev_cls] = fat[cls]; /* maintain the cluster chain */
		fib->fib_size -= SOS_CLUSTER_SIZE; /* update file size */
	} else
		fib->fib_size -= fib->fib_size % SOS_CLUSTER_SIZE; /* update file size */

	fat[cls] = SOS_FAT_ENT_FREE;  /* Free the cluster */

	/*
	 * Write back the FAT
	 */
	rc = write_fat_sword(ch, fatrec, &fat[0]);  /* write fat */
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/** Write the directory entry to the disk.
    @param[in] ch    The drive letter
    @param[in] dirps The record number of the first directory entry on the disk.
    @param[in] swd_fname The file name in SWORD(NOT C String)
    @param[out] fib  The destination address of the file information block
    @retval      0  Success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
 */
static int
write_dent_sword(sos_devltr ch, BYTE dirps, struct _storage_fib *fib){
	int                     rc;
	BYTE                   rec;
	BYTE          dirno_offset;
	WORD                 rwcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	/*
	 * Read directory entry
	 */
	rec = fib->fib_dirno / SOS_DENTRIES_PER_REC + dirps;
	rc = storage_record_read(ch, &buf[0], rec, 1, &rwcnt);
	if ( rc != 0 )
		goto error_out;  /* Error */

	if ( rwcnt != 1 ) {

		rc = SOS_ERROR_IO;
		goto error_out;  /* I/O Error */
	}

	/* Calculate dirno offset in the record */
	dirno_offset = fib->fib_dirno % SOS_DENTRIES_PER_REC;
	/* refer the directory entry to modify */
	dent = (BYTE *)&buf[0] + dirno_offset * SOS_DENTRY_SIZE;
	/* Modify the entry */
	STORAGE_FIB2DENT(fib, dent);

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

/** Search a file in the directory entry on the disk.
    @param[in] ch    The drive letter
    @param[in] dirps The record number of the first directory entry on the disk
    @param[in] swd_fname The file name in SWORD(NOT C String)
    @param[out] fib  The destination address of the file information block
 */
static int
search_dent_sword(sos_devltr ch, BYTE dirps, const BYTE *swd_fname,
    struct _storage_fib *fib){
	int                     rc;
	int                      i;
	BYTE                   rec;
	BYTE                 dirno;
	BYTE                  attr;
	BYTE                *fname;
	WORD                 rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	for(rec = dirps, dirno = 0; SOS_DENTRY_NR > dirno; ++rec) {

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

	/*
	 * The end of directory entry was not found.
	 */
	rc = SOS_ERROR_NOENT; /* File not found */
	goto error_out;

found:
	/*
	 * Fill the file information block
	 */
	STORAGE_FILL_FIB(fib,ch,rec,dirno,dent);

	return 0;

error_out:
	return rc;
}

/** Search a free directory entry on the disk.
    @param[in] ch    The drive letter
    @param[in] dirps The record number of the first directory entry on the disk
    @retval      0  Success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
 */
static int
search_free_dent_sword(sos_devltr ch, BYTE dirps, const BYTE *swd_fname, BYTE  *dirnop){
	int                     rc;
	int                      i;
	BYTE                   rec;
	BYTE                 dirno;
	BYTE                  attr;
	WORD                 rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	for(rec = dirps, dirno = 0; SOS_DENTRY_NR > dirno; ++rec) {

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

			if ( attr == SOS_FATTR_FREE )
				goto found; /* Free entry */

			if ( attr == SOS_FATTR_EODENT ) {

				rc = SOS_ERROR_NOENT; /* File not found */
				goto error_out;
			}
		}
	}

	/*
	 * The end of directory entry not found.
	 */
	rc = SOS_ERROR_NOENT; /* File not found */
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

/** Get the block in the file.
    @param[in] ch     The drive letter
    @param[in] fatrec The record number of FAT
    @param[in] fib    The file information block of the file contains the block
    @param[in] pos    The file position where the block is placed at
    @param[in] bufsiz The size of the buffer to store the contents of the block
    @param[in] mode
    @param[out] dest   The destination address of the buffer to write the contents of
    the block
    @param[out] blkp  The address to store the cluster number of the block
    FS_SWD_GTBLK_RD_FLG Get block to read
    FS_SWD_GTBLK_WR_FLG Get block to write
    @retval      0  Success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
 */
static int
get_block_sword(sos_devltr ch, BYTE fatrec, struct _storage_fib *fib, WORD pos,
    size_t bufsiz, int mode, BYTE *dest, WORD *blkp){
	int                     rc;
	int                      i;
	BYTE                   cls;
	WORD                 rwcnt;
	WORD                recoff;
	WORD                 blkno;
	WORD           blk_remains;
	size_t          rd_remains;
	BYTE  fat[SOS_RECORD_SIZE];
	BYTE  buf[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, fatrec, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/* How many blocks to be read */
	blk_remains = CALC_NEXT_ALIGN_Z80_WORD(pos % SOS_MAX_FILE_SIZE,
	    SOS_CLUSTER_SIZE) / SOS_CLUSTER_SIZE;

	for(cls = fib->fib_cls & SOS_FAT_CLSNUM_MASK; blk_remains > 0;
	    --blk_remains, cls = fat[cls]) {

		if ( cls == SOS_FAT_ENT_FREE ) {  /* Free entry */

			rc = SOS_ERROR_BADFAT;  /* Invalid cluster chain */
			goto error_out;
		}

		if ( SOS_IS_END_CLS(cls) ) {  /* End of cluster */

			if ( !FS_SWD_GETBLK_TO_WRITE(mode) ) { /* block read case */

				/* return the block not found error  */
				rc = SOS_ERROR_NOENT;
				goto error_out;
			}

			/*
			 * allocate new block
			 */
			rc = alloc_newblock_sword(ch, fatrec, &blkno);
			if ( rc != 0 )
				goto error_out;
			fat[cls] = blkno & SOS_FAT_CLSNUM_MASK; /* make cluster chain */
			continue;
		}
	}

	/*
	 * read cluster
	 */
	for(rd_remains = bufsiz, recoff = 0; rd_remains > 0 ;
	    rd_remains -= SOS_MIN(rd_remains, SOS_RECORD_SIZE), ++recoff) {

		rc = storage_record_read(ch, &buf[0],
		    SOS_CLS2REC(cls) + recoff, 1, &rwcnt);
		if ( rc != 0 )
			goto error_out;  /* Error */

		if ( rwcnt != 1 ) {

			rc = SOS_ERROR_IO;
			goto error_out;  /* I/O Error */
		}

		/*
		 * Copy contents of the block
		 */
		if ( dest != NULL )
			memcpy(dest + recoff * SOS_RECORD_SIZE,
			    &buf[0], SOS_MIN(rd_remains, SOS_RECORD_SIZE));
	}

	if ( blkp != NULL )
		*blkp = cls;  /* return the number of cluster */

	/* update file size */
	fib->fib_size = SOS_MAX(fib->fib_size, pos % SOS_MAX_FILE_SIZE);

	return 0;

error_out:
	return rc;
}

/** Put the block of the file.
    @param[in] ch     The drive letter
    @param[in] cls    The cluster number of the block
    @param[in] src    The destination address of the block buffer to write
    @param[in] bufsiz The size of the buffer to store the contents of the block
    @retval      0  Success
    @retval ENODEV  No such device
    @retval EINVAL  The device letter is not supported.
    @retval ENOENT  The device is not supported.
    @retval ENXIO   The device has not been mounted.
    @retval ENOSPC  File not found
    @retval ENOTBLK Block device required
    @retval EIO     I/O Error.
 */
static int
put_block_sword(sos_devltr ch, WORD cls, const void *src, size_t bufsiz, int mode){
	int                     rc;
	WORD                   rec;
	WORD                 rwcnt;
	size_t              remain;
	const void             *sp;
	BYTE  buf[SOS_RECORD_SIZE];


	for(sp = src, rec = SOS_CLS2REC(cls), remain = bufsiz; remain > 0;
	    remain += SOS_RECORD_SIZE, sp += SOS_RECORD_SIZE) {

		if ( SOS_RECORD_SIZE > remain ) {

			/*
			 * Modify the beginning of the buffer at the end of file
			 */
			rc = storage_record_read(ch, &buf[0], rec, 1, &rwcnt);
			if ( rc != 0 )
				goto error_out;  /* I/O Error */
			if ( rwcnt != 1 ) {

				rc = SOS_ERROR_IO;
				goto error_out;  /* I/O Error */
			}

			memcpy(&buf[0], sp, remain);  /* Modify */

			/*
			 * Write back
			 */
			rc = storage_record_write(ch, &buf[0], rec, 1, &rwcnt);
			if ( rc != 0 )
				goto error_out;  /* I/O Error */
			if ( rwcnt != 1 ) {

				rc = SOS_ERROR_IO;
				goto error_out;  /* I/O Error */
			}
			remain = 0;
		} else {

			/*
			 * Write back
			 */
			rc = storage_record_write(ch, sp, rec, 1, &rwcnt);
			if ( rc != 0 )
				goto error_out;  /* I/O Error */
			if ( rwcnt != 1 ) {

				rc = SOS_ERROR_IO;
				goto error_out;  /* I/O Error */
			}
		}
	}

	return 0;

error_out:
	return rc;
}

/*
 * Interface functions
 */
