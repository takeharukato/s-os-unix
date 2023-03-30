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

#include "sim-type.h"
#include "misc.h"
#include "compat.h"
#include "sos.h"
#include "storage.h"
#include "fs-vfs.h"
#include "fs-sword.h"

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
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
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
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
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
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
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
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOSPC  Device full
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
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
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
    @param[in] fib   The address of the file information block
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
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
/** Read the directory entry by #DIRNO
    @param[in]   ch        The drive letter
    @param[in]   dirps     The record number of the first directory entry on the disk
    @param[in]   dirno     The #DIRNO number of the directory entry to read.
    @param[out]  recp      The address to store the record number of the directory entry.
    @param[out]  dentp     The address to store the directory entry.
    @param[in]   bufsiz    The size of the buffer pointed by DENTP.
    @retval     0               Success
    @retval     SOS_ERROR_IO    I/O Error
    @retval     SOS_ERROR_NOENT File not found
 */
static int
read_dent_sword(sos_devltr ch, BYTE dirps, BYTE dirno, BYTE *recp, BYTE *dentp, size_t bufsiz){
	int                      i;
	int                     rc;
	BYTE                   cur;
	BYTE                   rec;
	BYTE                  attr;
	WORD                 rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	for(rec = dirps, cur = 0; SOS_DENTRY_NR > cur; ++rec) {

		/*
		 * Read each directory entry
		 */

		/* Read a directory entry record */
		rc = storage_record_read(ch, &buf[0], rec, 1, &rdcnt);
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

			if ( cur == dirno ) {

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
/** Search a file in the directory entry on the disk.
    @param[in] ch     The drive letter
    @param[in] dirps  The record number of the first directory entry on the disk
    @param[in] swd_fname The file name in SWORD(NOT C String)
    @param[out] fib  The destination address of the file information block
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
static int
search_dent_sword(sos_devltr ch, BYTE dirps, const BYTE *swd_fname,
    struct _storage_fib *fib){
	int                         rc;
	BYTE                       rec;
	BYTE                     dirno;
	BYTE     dent[SOS_DENTRY_SIZE];

	for(dirno = 0; SOS_DENTRY_NR > dirno; ++dirno) {

		/* Read each directory entry. */
		rc = read_dent_sword(ch, dirps, dirno, &rec, &dent[0], SOS_DENTRY_SIZE);
		if ( rc != 0 )
			goto error_out; /* File not found */

		if ( memcmp(&dent[0] + SOS_FIB_OFF_FNAME, &swd_fname[0],
			SOS_FNAME_NAMELEN) == 0 )
			goto found;  /* Found */
	}

	sos_assert_no_reach();  /* This function never comes here */

found:
	/*
	 * Fill the file information block
	 */
	if ( fib != NULL )
		STORAGE_FILL_FIB(fib, ch, dirno, &dent[0]);

	return 0;

error_out:
	return rc;
}

/** Search a free directory entry on the disk.
    @param[in] ch    The drive letter
    @param[in] dirps The record number of the first directory entry on the disk
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
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

/** Get the cluster number of the block from the file position of the file
    @param[in]  ch     The drive letter
    @param[in]  fatrec The record number of FAT
    @param[in]  fib    The file information block of the file contains the block
    @param[in]  pos    The file position where the block is placed at
    @param[out] clsp   The address to store the cluster number.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
get_cluster_number_sword(sos_devltr ch, BYTE fatrec, struct _storage_fib *fib,
    WORD pos, int mode, BYTE *clsp){
	int                     rc;
	BYTE                   cls;
	WORD                 blkno;
	WORD           blk_remains;
	BYTE  fat[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, fatrec & SOS_FAT_CLSNUM_MASK, &fat[0]);  /* read fat */
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
	if ( clsp != NULL )
		*clsp = cls;
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
    @param[in] mode   The number to specify the behavior.
    FS_SWD_GTBLK_RD_FLG Get block to read
    FS_SWD_GTBLK_WR_FLG Get block to write
    @param[out] dest   The destination address of the buffer to write the contents of
    the block
    @param[out] blkp  The address to store the cluster number of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
get_block_sword(sos_devltr ch, BYTE fatrec, struct _storage_fib *fib, WORD pos,
    size_t bufsiz, int mode, BYTE *dest, WORD *blkp){
	int                     rc;
	int                      i;
	BYTE                   cls;
	WORD                 rwcnt;
	WORD                recoff;
	size_t          rd_remains;
	BYTE  buf[SOS_RECORD_SIZE];

	/*
	 * Get the cluster number of POS
	 */
	rc = get_cluster_number_sword(ch, fatrec, fib, pos, mode, &cls);
	if ( rc != 0 )
		goto error_out;

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
    @param[in] fatrec The record number of FAT
    @param[in] fib    The file information block of the file contains the block
    @param[in] src    The destination address of the block buffer to write
    @param[in] bufsiz The size of the buffer to store the contents of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
put_block_sword(sos_devltr ch, BYTE fatrec, struct _storage_fib *fib, WORD pos,
    const void *src, size_t bufsiz){
	int                     rc;
	WORD                   rec;
	WORD                 rwcnt;
	BYTE                   cls;
	size_t              remain;
	const void             *sp;
	BYTE  buf[SOS_RECORD_SIZE];

	/*
	 * Get the cluster number of POS
	 */
	rc = get_cluster_number_sword(ch, fatrec, fib, pos, FS_SWD_GTBLK_WR_FLG, &cls);
	if ( rc != 0 )
		goto error_out;

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
 * File system operations
 */

/** Open a file
    @param[in] ch       The drive letter
    @param[in] fname    The filename to open
    @param[out] fibp     The address to store the file information block
    @param[out] privatep The pointer to the pointer variable to store
    the private information
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fops_open_sword(sos_devltr ch, const char *fname, struct _storage_fib *fibp, void **privatep){
	int                           rc;
	fs_dirps                   dirps;
	struct _storage_fib          fib;
	BYTE    swd_fname[SOS_FNAME_LEN];

	/*
	 * conver the filename which was inputted from the console to
	 * the sword filename format.
	 */
	rc = fs_unix2sword(fname, &swd_fname[0], SOS_FNAME_LEN);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search file from directory entry.
	 */

	rc = storage_get_dirps(ch, &dirps);  /* Get current #DIRPS */
	if ( rc != 0 )
		goto error_out;

	rc = search_dent_sword(ch, dirps, &swd_fname[0], &fib);
	if ( rc != 0 )
		goto error_out;

	/*
	 * return file information block
	 */
	if ( fibp != NULL )
		memcpy(fibp, &fib, sizeof(struct _storage_fib));

	return 0;

error_out:
	return rc;
}

/** Close the file
    @param[in] fdp  The file descriptor to close
    @retval      0  Success
 */
int
fops_close_sword(struct _sword_file_descriptor *fdp){

	return 0;
}

/** Read from the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] dest   The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] rdsizp  The adress to store read bytes.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fops_read_sword(struct _sword_file_descriptor *fdp, void *dest, size_t count,
    size_t *rdsizp){
	int                        rc;
	void                      *dp;
	struct _storage_disk_pos *pos;
	size_t                 remain;
	fs_off_t                  off;
	BYTE clsbuf[SOS_CLUSTER_SIZE];
	BYTE                   fatrec;

	pos = &fdp->fd_pos;
	fatrec =(BYTE)( pos->dp_fatpos & 0xff );

	for(dp = dest, off = pos->dp_pos, remain = count; remain > 0; ) {

		rc = get_block_sword(pos->dp_devltr, fatrec,
		    &fdp->fd_fib, off, SOS_CLUSTER_SIZE,
		    FS_SWD_GTBLK_RD_FLG, &clsbuf[0], NULL);
		if ( rc != 0 )
			goto error_out;

		/*
		 * Copy data
		 */
		if ( SOS_CLUSTER_SIZE > remain ) {

			/* read and copy the record */
			memcpy(dp, &clsbuf[0], SOS_CLUSTER_SIZE);
			off += SOS_CLUSTER_SIZE;
			dp += SOS_CLUSTER_SIZE;
			remain -= SOS_CLUSTER_SIZE;
		} else {

			/* read the record and copy the remaining bytes. */
			memcpy(dp, &clsbuf[0], remain);
			off += remain;
			dp += remain;
			remain = 0;
		}
	}

	rc = 0;

error_out:
	if ( rdsizp != NULL )
		*rdsizp = count - remain;

	return rc;
}

/** Write to the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] src    The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] wrsizp  The adress to store written bytes.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fops_write_sword(struct _sword_file_descriptor *fdp, const void *src,
    size_t count, size_t *wrsizp){
	int                        rc;
	const void                *sp;
	BYTE                   fatrec;
	struct _storage_disk_pos *pos;
	fs_off_t                  off;
	size_t                 remain;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	pos = &fdp->fd_pos;
	fatrec =(BYTE)( pos->dp_fatpos & 0xff );

	for(sp = src, off = pos->dp_pos, remain = count; remain > 0; ) {

		/*
		 * Write data to a block
		 */
		if ( SOS_CLUSTER_SIZE > remain ) {

			/* Write the whole record. */
			rc = put_block_sword(pos->dp_devltr,
			    fatrec, &fdp->fd_fib, off, sp, SOS_CLUSTER_SIZE);
			if ( rc != 0 )
				goto error_out;

			sp += SOS_CLUSTER_SIZE;
			off += SOS_CLUSTER_SIZE;
			remain -= SOS_CLUSTER_SIZE;
		} else {

			/* Read the record and write the remaining
			 * bytes from the beginning of the cluster.
			 */

			/* Read the contents of the last cluster. */
			rc = get_block_sword(pos->dp_devltr,
			    fatrec, &fdp->fd_fib, pos->dp_pos, SOS_CLUSTER_SIZE,
			    FS_SWD_GTBLK_WR_FLG, &clsbuf[0], NULL);
			if ( rc != 0 )
				goto error_out;

			memcpy(&clsbuf[0], sp, remain);

			/* Write the remaining data from
			 * the beginning of the cluster.
			 */
			rc = put_block_sword(pos->dp_devltr,
			    fatrec, &fdp->fd_fib, pos->dp_pos,
			    &clsbuf[0], SOS_CLUSTER_SIZE);
			if ( rc != 0 )
				goto error_out;

			sp += remain;
			off += remain;
			remain = 0;
		}
	}

	rc = 0;

error_out:
	if ( wrsizp != NULL )
		*wrsizp = count - remain;

	return rc;
}

/** Stat the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] fib    The buffer to store the file information block of the file.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fops_stat_sword(struct _sword_file_descriptor *fdp, struct _storage_fib *fib){

	/* Copy the file infomation block */
	memmove(fib, &fdp->fd_fib, sizeof(struct _storage_fib));
}

/** Reposition read/write file offset
    @param[in]  fdp     The file descriptor to the file.
    @param[in]  offset  The offset to reposition according to WHENCE.
    @param[in]  whence  The directive to reposition:
     FS_VFS_SEEK_SET The file offset is set to offset bytes.
     FS_VFS_SEEK_CUR The file offset is set to its current location plus offset bytes.
     FS_VFS_SEEK_END The file offset is set to the size of the file plus offset bytes.
    @param[out] new_pos
    @retval     0                Success
    @retval     EINVAL           Invalid whence
    @retval     ENXIO            The new position exceeded the file size
 */
int
fops_seek_sword(struct _sword_file_descriptor *fdp, fs_off_t offset, int whence,
    fs_off_t *new_pos ){
	fs_off_t                  new;
	fs_off_t                  cur;
	fs_off_t                  off;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;

	pos = &fdp->fd_pos;  /* Position information */
	fib = &fdp->fd_fib;  /* File information block */

	/* Adjust offset */
	if ( offset > 0 )
		off = SOS_MIN(offset, SOS_MAX_FILE_SIZE);
	else if ( 0 > offset )
		off = SOS_MAX(offset, (fs_off_t)-1 * SOS_MAX_FILE_SIZE);

	/*
	 * Calculate the start position
	 */
	switch( whence ) {

	case FS_VFS_SEEK_SET:
		cur = 0;
		break;

	case FS_VFS_SEEK_CUR:
		cur = SOS_MIN(pos->dp_pos, SOS_MAX_FILE_SIZE);
		break;

	case FS_VFS_SEEK_END:
		cur = SOS_MIN(fib->fib_size, SOS_MAX_FILE_SIZE);
		break;

	default:
		return EINVAL;
	}

	if ( 0 > ( cur + offset ) )
		new = 0;
	else if ( offset > ( SOS_MAX_FILE_SIZE - cur ) )
		new = SOS_MAX_FILE_SIZE;
	else
		new = cur + offset;

	if ( new_pos != NULL )
		*new_pos = new;

	return 0;
}

/** Open directory
    @param[out] dir     The pointer to the DIR structure (directory stream).
    @retval     0       Success
    @retval     EINVAL  Invalid whence
    @retval     ENXIO   The new position exceeded the file size
    @remark     DIRP has been initialized by the caller and this function is
    responsible for setting the dir_pos member of the dir_pos structured
    variable in DIR  and the private information.
 */
int
fops_opendir_sword(struct _sword_dir *dir){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	pos->dp_pos = 0;  /* Set to the first directory entry */
	pos->dp_private = NULL; /* Init private information */

	return 0;
}

/** Read the directory
    @param[in]  dir  The pointer to the DIR structure (directory stream).
    @param[out] fib  The pointer to the file information block.
    @retval     0      Success
    @retval     EINVAL Invalid whence
    @retval     ENXIO  The new position exceeded the file size
    @remark     This function is responsible for setting the dir_pos member of
    the dir_pos structured variable in DIR and filling the FIB.
    Other members in the dir_pos should be set by the caller.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
    The function returns the file information block at the current position
    indicated by the dir_pos member of the dir_pos structured variable in DIR.
 */
int
fops_readdir_sword(struct _sword_dir *dir, struct _storage_fib *fib){
	int                        rc;
	struct _storage_disk_pos *pos;
	BYTE                    dirno;
	BYTE                      rec;
	BYTE    dent[SOS_DENTRY_SIZE];
	fs_dirps                dirps;

	pos = &dir->dir_pos;  /* Position information */

	/*
	 * read current entry
	 */
	dirno = pos->dp_pos / SOS_DENTRY_SIZE;  /* Set #DIRNO up */

	rc = storage_get_dirps(pos->dp_devltr, &dirps);  /* Get current #DIRPS */
	if ( rc != 0 )
		goto error_out;

	dirps &= 0xff; /* The size of #DIRPS in the SWORD is BYTE. */
	rc = read_dent_sword(pos->dp_devltr, dirps,
	    dirno, &rec, &dent[0], SOS_DENTRY_SIZE);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Fill the file information block
	 */
	if ( fib != NULL )
		STORAGE_FILL_FIB(fib, pos->dp_devltr, dirno, &dent[0]);

	/*
	 * Update positions
	 *
	 * @remark This function regards a directory as a binary file containing
	 * an array of directory entries, it sets dir_pos only.
	 */
	pos->dp_pos = ( dirno + 1 ) * SOS_DENTRY_SIZE;  /* file position */

	return 0;

error_out:
	return rc;
}

/** Set the position of the next fs_readdir() call in the directory stream.
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[in]  dirno  The position of the next fs_readdir() call
    It should be a value returned by a previous call to fs_telldir.
    @retval     0      Success
    @retval     EINVAL Invalid dirno
    @retval     ENXIO  The new position exceeded the SOS_DENTRY_NR.
    @remark     This function is responsible for setting the dir_pos member of
    the dir_pos structured variable in DIR and filling the FIB.
    Other members in the dir_pos should be set by the caller.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
 */
int
fops_seekdir_sword(struct _sword_dir *dir, fs_dirno dirno){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	if ( 0 > dirno )
		return EINVAL;  /* Invalid #DIRNO */

	if ( dirno > SOS_DENTRY_NR )
		return ENXIO;   /* #DIRNO is out of range. */

	pos->dp_pos = dirno * SOS_DENTRY_SIZE;  /* set seek position */

	return 0;
}

/** Return current location in directory stream
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[in]  dirno  The position of the next fs_readdir() call
    @param[out] dirnop The address to store current location in directory stream.
    It should be a value returned by a previous call to fs_telldir.
    @retval     0      Success
    @retval     EINVAL Invalid dirno
    @retval     ENXIO  The new position exceeded the SOS_DENTRY_NR.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
 */
int
fops_telldir_sword(const struct _sword_dir *dir, fs_dirno *dirnop){
	const struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	if ( dirnop == NULL )
		goto out;

	*dirnop = pos->dp_pos / SOS_DENTRY_SIZE;  /* current position */

out:
	return 0;
}

/** close the directory
    @param[in]  dir  The pointer to the DIR structure (directory stream).
    @retval     0      Success
    @retval     EINVAL Invalid whence
    @retval     ENXIO  The new position exceeded the file size
 */
int
fops_closedir_sword(struct _sword_dir *dir){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	pos->dp_pos = 0;        /* Reset position */
	pos->dp_private = NULL; /* Clear private information */
}

/** change the name of a file
    @param[in]  dir     The pointer to the DIR structure (directory stream).
    @param[in]  oldpath The filename to be changed
    @param[in]  newpath The filename to change to.
    @retval     0               Success
    @retval     SOS_ERROR_EXIST Newpath already exists
    @retval     SOS_ERROR_NOENT Oldpath is not Found.
 */
int
fops_rename(struct _sword_dir *dir, const unsigned char *oldpath,
    const unsigned char *newpath){
	int                             rc;
	fs_dirps                     dirps;
	struct _storage_disk_pos      *pos;
	struct _storage_fib        old_fib;
	struct _storage_fib        new_fib;
	BYTE    old_swdname[SOS_FNAME_LEN];
	BYTE    new_swdname[SOS_FNAME_LEN];
	BYTE         dent[SOS_DENTRY_SIZE];

	pos = &dir->dir_pos;  /* Position information */

	rc = storage_get_dirps(pos->dp_devltr, &dirps);  /* Get current #DIRPS */
	if ( rc != 0 )
		goto error_out;

	/* Get the filename of oldpath in SWORD representation. */
	rc = fs_unix2sword(oldpath, &old_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to be renamed. */
	rc = search_dent_sword(pos->dp_devltr, dirps, &old_swdname[0], &old_fib);
	if ( rc != 0 )
		goto error_out;

	/* Get the filename of newpath in SWORD representation. */
	rc = fs_unix2sword(newpath, &new_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Confirm the new filename doesn't exist.
	 */
	rc = search_dent_sword(pos->dp_devltr, dirps, &new_swdname[0], &new_fib);
	if ( rc == 0 ) {

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/* Change the file name */
	memcpy(&old_fib.fib_sword_name[0],&new_swdname[0],SOS_FNAME_LEN);

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, dirps, &old_fib);
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}
