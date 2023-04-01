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

/** Determine whether the open flags is invalid
    @param[in] _attr The file attribute in the file information block or
    the directory entry.
   @param[in] _f The open flags
   @retval TRUE  The open flags is invalid
   @retval FALSE The open flags is valid
 */
#define FS_IS_OPEN_FLAGS_INVALID(_attr, _f)				\
        ( ( ( (_f) & FS_VFS_FD_FLAG_MAY_WRITE ) == FS_VFS_FD_FLAG_O_CREAT ) || \
	    !SOS_FATTR_IS_VALID(_attr) )


/*
 * Foward declarations
 */

/*
 * Internal functions
 */

/** Get dirps and fatpos from storage
    @param[in]   ch       The device letter of the device
    @param[out]  dirpsp   The address to store #DIRPS
    @param[out]  fatposp  The address to store #FATPOS
    @retval    0                Success
    @retval    SOS_ERROR_OFFLINE Can not get #DIRPS or #FATPOS
 */
static int
get_dirps_and_fatpos(sos_devltr ch, BYTE *dirpsp, BYTE *fatposp){
	int                        rc;
	fs_fatpos              fatpos;
	fs_dirps                dirps;

	rc = storage_get_fatpos(ch, &fatpos);
	if ( rc != 0 )
		goto error_out;

	rc = storage_get_dirps(ch, &dirps);
	if ( rc != 0 )
		goto error_out;

	if ( dirpsp != NULL )
		*dirpsp = SOS_DIRPS_VAL(dirps);

	if ( fatposp != NULL )
		*fatposp = SOS_FATPOS_VAL(fatpos);

	return 0;

error_out:
	return rc;
}

/** Read file allocation table (FAT)
    @param[in]  ch  The device letter of the device
    @param[out] fatbuf Memory buffer for the FAT
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
 */
static int
read_fat_sword(sos_devltr ch, void *fatbuf){
	int      rc;
	WORD  rdcnt;
	BYTE fatrec;

	rc = get_dirps_and_fatpos(ch, NULL, &fatrec);
	if ( rc != 0 )
		goto error_out;

	rc = storage_record_read(ch, fatbuf,
	    fatrec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &rdcnt);

	if ( rc != 0 )
		return rc;

	if ( rdcnt != SOS_FAT_SIZE/SOS_RECORD_SIZE )
		return SOS_ERROR_IO;

	return 0;

error_out:
	return rc;
}

/** Write file allocation table (FAT)
    @param[in]  ch  The device letter of the device
    @param[in] fatbuf Memory buffer for the FAT
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
 */
static int
write_fat_sword(sos_devltr ch, const void *fatbuf){
	int      rc;
	WORD  wrcnt;
	BYTE fatrec;

	rc = get_dirps_and_fatpos(ch, NULL, &fatrec);
	if ( rc != 0 )
		goto error_out;

	rc = storage_record_write(ch, fatbuf,
	    fatrec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &wrcnt);

	if ( rc != 0 )
		return rc;

	if ( wrcnt != SOS_FAT_SIZE/SOS_RECORD_SIZE )
		return SOS_ERROR_IO;

	return 0;

error_out:
	return rc;
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
	WORD                   rec;
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
    @param[in]  pos     The file offset position
    @param[in]  use_recs The used record numbers at the last cluster
    @param[out] blknop  The address to store the block number of the new block.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOSPC  Device full
  */
static int
alloc_newblock_sword(sos_devltr ch, BYTE use_recs, WORD *blknop){
	int                     rc;
	int                      i;
	BYTE                 *clsp;
	BYTE  fat[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search a free cluster.
	 */
	for( i = 0, clsp = &fat[0]; SOS_FAT_NR > i; ++i, clsp = &fat[i]){

		if ( *clsp == SOS_FAT_ENT_FREE ) {

			/* alloc new cluster and fill used records */
			if ( use_recs > 0 )
				*clsp = SOS_FAT_ENT_EOF_MASK|(use_recs - 1);
			else
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

	rc = write_fat_sword(ch, &fat[0]);  /* write fat */
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
    @param[in] fib    The file information block of the file contains the block
    @param[in] newsiz The file length of the file to be truncated.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
static int
release_block_sword(sos_devltr ch, struct _storage_fib *fib, WORD newsiz) {
	int                     rc;
	int                 relcnt;
	BYTE                clsoff;
	BYTE              prev_cls;
	BYTE              next_cls;
	BYTE          last_cls_ptr;
	WORD                   cls;
	WORD               rel_cls;
	BYTE              used_rec;
	BYTE                fatrec;
	BYTE  fat[SOS_RECORD_SIZE];

	rc = get_dirps_and_fatpos(ch, NULL, &fatrec);
	if ( rc != 0 )
		goto error_out;

	rc = read_fat_sword(ch, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/*
	 * Find the start block to release
	 */

	/* How many clusters to search */
	clsoff = SOS_CALC_ALIGN_Z80_WORD(newsiz % SOS_MAX_FILE_SIZE, SOS_CLUSTER_SIZE);

	/*
	 * Search the cluster to free.
	 */
	for( cls = SOS_CLS_VAL(fib->fib_cls), prev_cls = cls, next_cls=fat[cls];
	     clsoff > 0; prev_cls = cls, cls = next_cls, next_cls = fat[next_cls],
		     --clsoff) {

		if ( cls == SOS_FAT_ENT_FREE ) {

			rc = SOS_ERROR_BADFAT;  /* Invalid cluster chain */
			goto error_out;
		}

		if ( SOS_IS_END_CLS(cls) ) {

			/* return the block not found error  */
			rc = SOS_ERROR_NOENT;
			goto error_out;
		}
	}

	/*
	 * Handle the last cluster of the file
	 */
	sos_assert( !SOS_IS_END_CLS(prev_cls) );
	last_cls_ptr = fat[prev_cls];
	used_rec = newsiz / SOS_RECORD_SIZE;  /* Calculate used records */
	if ( newsiz == 0 )
		fib->fib_cls = SOS_FAT_ENT_EOF_MASK;
	else
		fat[prev_cls] = SOS_FAT_ENT_EOF_MASK|used_rec;


	/*
	 * Release blocks
	 */
	if ( ( fib->fib_size - newsiz ) > SOS_CLUSTER_SIZE ) {

		for(rel_cls = last_cls_ptr,
			    relcnt= CALC_NEXT_ALIGN_Z80_WORD(fib->fib_size - newsiz,
				SOS_CLUSTER_SIZE);
		    relcnt > 0; --relcnt, rel_cls = next_cls){

			next_cls = fat[rel_cls];  /* remind next cluster */
			sos_assert( next_cls != SOS_FAT_ENT_FREE );

			fat[rel_cls] = SOS_FAT_ENT_FREE; /* free cluster */
			if ( SOS_IS_END_CLS(next_cls) )
				break;
		}
	}

	rc = write_fat_sword(ch, &fat[0]);  /* write fat */
	if ( rc != 0 )
		goto error_out;

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
static int
write_dent_sword(sos_devltr ch, struct _storage_fib *fib){
	int                     rc;
	BYTE                   rec;
	BYTE          dirno_offset;
	BYTE             dirps_rec;
	WORD                 rwcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	rc = get_dirps_and_fatpos(ch, &dirps_rec, NULL);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Read directory entry
	 */
	rec = fib->fib_dirno / SOS_DENTRIES_PER_REC + dirps_rec;
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
read_dent_sword(sos_devltr ch, BYTE dirno, BYTE *recp, BYTE *dentp, size_t bufsiz){
	int                      i;
	int                     rc;
	BYTE                   cur;
	BYTE                   rec;
	BYTE                  attr;
	WORD                 rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];
	BYTE             dirps_rec;

	rc = get_dirps_and_fatpos(ch, &dirps_rec, NULL);
	if ( rc != 0 )
		goto error_out;

	for(rec = dirps_rec, cur = 0; SOS_DENTRY_NR > cur; ++rec) {

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
    @param[in] swd_name The file name in SWORD(NOT C String)
    @param[out] fib  The destination address of the file information block
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
static int
search_dent_sword(sos_devltr ch, const BYTE *swd_name, struct _storage_fib *fib){
	int                         rc;
	BYTE                       rec;
	BYTE                     dirno;
	BYTE     dent[SOS_DENTRY_SIZE];

	for(dirno = 0; SOS_DENTRY_NR > dirno; ++dirno) {

		/* Read each directory entry. */
		rc = read_dent_sword(ch, dirno, &rec, &dent[0], SOS_DENTRY_SIZE);
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
		STORAGE_FILL_FIB(fib, ch, dirno, &dent[0]);

	return 0;

error_out:
	return rc;
}

/** Search a free directory entry on the disk.
    @param[in] ch     The drive letter
    @param[out] dirnop The the address to store #DIRNO of the found entry.
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOSPC Free entry not found
 */
static int
search_free_dent_sword(sos_devltr ch, BYTE *dirnop){
	int                     rc;
	int                      i;
	BYTE                   rec;
	BYTE                 dirno;
	BYTE             dirps_rec;
	BYTE                  attr;
	WORD                 rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	rc = get_dirps_and_fatpos(ch, &dirps_rec, NULL);
	if ( rc != 0 )
		goto error_out;

	for(rec = dirps_rec, dirno = 0; SOS_DENTRY_NR > dirno; ++rec) {

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

				rc = SOS_ERROR_NOSPC; /* Device Full */
				goto error_out;
			}
		}
	}

	/*
	 * The end of directory entry not found.
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

/** Get the cluster number of the block from the file position of the file
    @param[in]  ch     The drive letter
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
get_cluster_number_sword(sos_devltr ch, struct _storage_fib *fib,
    WORD pos, int mode, BYTE *clsp){
	int                     rc;
	BYTE                   cls;
	WORD                 blkno;
	WORD           blk_remains;
	BYTE              use_recs;
	BYTE  fat[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/* How many blocks to be read */
	blk_remains = CALC_NEXT_ALIGN_Z80_WORD(pos % SOS_MAX_FILE_SIZE,
	    SOS_CLUSTER_SIZE) / SOS_CLUSTER_SIZE;

	for(cls = SOS_CLS_VAL(fib->fib_cls); blk_remains > 0;
	    --blk_remains) {

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
			if ( blk_remains > 1 ) /* use all cluster */
				use_recs = (BYTE)SOS_REC_VAL(SOS_CLUSTER_RECS);
			else
				use_recs =
					(BYTE)SOS_REC_VAL( ( pos % SOS_CLUSTER_SIZE )
					    / SOS_RECORD_SIZE ) + 1;

			rc = alloc_newblock_sword(ch, use_recs, &blkno);
			if ( rc != 0 )
				goto error_out;
			if ( SOS_IS_END_CLS(fib->fib_cls) ) {

				fib->fib_cls = SOS_CLS_VAL(blkno); /* first cluster */
				cls = fib->fib_cls;
			} else {

				fat[cls] = SOS_CLS_VAL(blkno); /* make cluster chain */
				cls = fat[cls];
			}
			continue;
		}
		cls = fat[cls];
	}
	if ( clsp != NULL )
		*clsp = cls;
	return 0;

error_out:
	return rc;
}

/** Get the block in the file.
    @param[in] ch     The drive letter
    @param[in] fib    The file information block of the file contains the block
    @param[in] pos    The file position where the block is placed at
    @param[in] mode   The number to specify the behavior.
    FS_SWD_GTBLK_RD_FLG Get block to read
    FS_SWD_GTBLK_WR_FLG Get block to write
    @param[out] dest   The destination address of the buffer to write the contents of
    the block
    @param[in] bufsiz The size of the buffer to store the contents of the block
    @param[out] blkp  The address to store the cluster number of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
get_block_sword(sos_devltr ch, struct _storage_fib *fib, WORD pos,
    int mode, BYTE *dest, size_t bufsiz, WORD *blkp){
	int                     rc;
	BYTE                   cls;
	WORD                 rwcnt;
	WORD                recoff;
	size_t          rd_remains;
	BYTE  buf[SOS_RECORD_SIZE];

	/*
	 * Get the cluster number of POS
	 */
	rc = get_cluster_number_sword(ch, fib, pos, mode, &cls);
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
    @param[in] fib    The file information block of the file contains the block
    @param[in] src    The destination address of the block buffer to write
    @param[in] bufsiz The size of the buffer to store the contents of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
put_block_sword(sos_devltr ch, struct _storage_fib *fib, WORD pos,
    const void *src, size_t bufsiz){
	int                     rc;
	WORD                   rec;
	WORD                 rwcnt;
	BYTE                   cls;
	size_t             remains;
	const void             *sp;
	BYTE  buf[SOS_RECORD_SIZE];

	/*
	 * Get the cluster number of POS
	 */
	rc = get_cluster_number_sword(ch, fib, pos, FS_SWD_GTBLK_WR_FLG, &cls);
	if ( rc != 0 )
		goto error_out;

	for(sp = src, rec = SOS_CLS2REC(cls), remains = bufsiz; remains > 0;
	    remains -= SOS_RECORD_SIZE, sp += SOS_RECORD_SIZE) {

		if ( SOS_RECORD_SIZE > remains ) {

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

			memcpy(&buf[0], sp, remains);  /* Modify */

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
			remains = 0;
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

/** Truncate a file to a specified length
    @param[in]  fib    The file information block of the file.
    @param[in]  pos    The file position information
    @param[in]  off    The file length of the file to be truncated.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
static int
change_filesize(struct _storage_fib *fib, struct _storage_disk_pos *pos,
    fs_off_t off){
	int                        rc;
	BYTE clsbuf[SOS_CLUSTER_SIZE];
	WORD                   newsiz;
	WORD                  realsiz;
	WORD                  remains;

	if ( ( off > 0 ) || ( off > SOS_MAX_FILE_SIZE ) )
		return SOS_ERROR_SYNTAX;

	newsiz = off;
	realsiz = fib->fib_size;
	if (  realsiz > newsiz ) {

		/*
		 * Release file blocks
		 */
		rc = release_block_sword(fib->fib_devltr, fib, newsiz);
		if ( rc != 0 )
			goto error_out;

	} else {

		remains = newsiz - realsiz;
		if ( SOS_CLUSTER_SIZE > remains ) {

			/*
			 * extend records at the last cluster
			 */

			/* Read block */
			rc = get_block_sword(pos->dp_devltr,
			    fib, realsiz, FS_SWD_GTBLK_RD_FLG,
			    &clsbuf[0], SOS_CLUSTER_SIZE, NULL);
			if ( rc != 0 )
				goto error_out;
			memset(&clsbuf[0] + realsiz % SOS_CLUSTER_SIZE,
			    0x0,
			    remains);  /* Clear newly allocated buffer. */
			/* update culster */
			rc = put_block_sword(pos->dp_devltr,
			    fib, realsiz,
			    &clsbuf[0], SOS_CLUSTER_SIZE);
			if ( rc != 0 )
				goto error_out;
		} else {

			/* alloc new blocks to the newsize. */
			rc = get_block_sword(pos->dp_devltr,
			    fib, newsiz, FS_SWD_GTBLK_WR_FLG,
			    &clsbuf[0], SOS_CLUSTER_SIZE, NULL);
			if ( rc != 0 )
				goto error_out;
		}
	}

	/*
	 * update file information block
	 */
	fib->fib_size = newsiz;  /* update size */
	rc = write_dent_sword(pos->dp_devltr, fib); /* write back */
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/*
 * File system operations
 */

/** Create a file
    @param[in] ch       The drive letter
    @param[in] fname    The filename to open
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    FS_VFS_FD_FLAG_O_ASC     Open/Create a ascii file
    FS_VFS_FD_FLAG_O_BIN     Open/Create a binary file
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] fibp     The address to store the file information block
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    *    SOS_ERROR_IO     I/O Error
    *    SOS_ERROR_EXIST  File Already Exists
    *    SOS_ERROR_NOSPC  Device Full (No free directory entry)
    *    SOS_ERROR_SYNTAX Invalid flags
 */
int
fops_creat_sword(sos_devltr ch, const unsigned char *fname, WORD flags,
    const struct _sword_header_packet *pkt, struct _storage_fib *fibp, BYTE *resp){
	int                           rc;
	BYTE                       dirno;
	struct _storage_fib          fib;
	BYTE     swd_name[SOS_FNAME_LEN];
	BYTE    clsbuf[SOS_CLUSTER_SIZE];

	if ( FS_IS_OPEN_FLAGS_INVALID(pkt->hdr_attr, flags) )
		return SOS_ERROR_SYNTAX;  /*  Invalid flags  */

	/*
	 * convert the filename which was inputted from the console to
	 * the sword filename format.
	 */
	rc = fs_unix2sword(fname, &swd_name[0], SOS_FNAME_LEN);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search file from directory entry.
	 */
	rc = search_dent_sword(ch, &swd_name[0], &fib);
	if ( rc == 0 ) {

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/*
	 * Search a free entry
	 */
	rc = search_free_dent_sword(ch, &dirno);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOSPC;
		goto error_out;
	}

	/*
	 * create the new file
	 */

	/*
	 * Set the file information block up
	 */
	fib.fib_devltr = ch;
	fib.fib_attr = SOS_FATTR_GET_FTYPE(pkt->hdr_attr);
	fib.fib_dirno = dirno;
	fib.fib_size = 0;
	fib.fib_dtadr = pkt->hdr_dtadr;
	fib.fib_exadr = pkt->hdr_exadr;
	fib.fib_cls = SOS_FAT_ENT_EOF_MASK; /* No FAT alloced */
	memcpy(&fib.fib_sword_name[0],&swd_name[0],SOS_FNAME_LEN);

	/*
	 * Set file type
	 */

	/* Update the directory entry. */
	rc = write_dent_sword(ch, &fib);
	if ( rc != 0 )
		goto error_out;

	/*
	 * return file information block
	 */
	if ( fibp != NULL )
		memcpy(fibp, &fib, sizeof(struct _storage_fib));

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}
/** Open a file
    @param[in] ch       The drive letter
    @param[in] fname    The filename to open
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    FS_VFS_FD_FLAG_O_ASC     Open/Create a ascii file
    FS_VFS_FD_FLAG_O_BIN     Open/Create a binary file
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] fibp     The address to store the file information block
    @param[out] privatep The pointer to the pointer variable to store
    the private information
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_EXIST  File Already Exists
    * SOS_ERROR_NOENT  File not found
    * SOS_ERROR_NOSPC  Device Full (No free directory entry)
    * SOS_ERROR_RDONLY Write proteced file
    * SOS_ERROR_SYNTAX Invalid flags
 */
int
fops_open_sword(sos_devltr ch, const unsigned char *fname, WORD flags,
    const struct _sword_header_packet *pkt, struct _storage_fib *fibp,
    void **privatep, BYTE *resp){
	int                           rc;
	struct _storage_fib          fib;
	BYTE                         res;
	BYTE     swd_name[SOS_FNAME_LEN];

	if ( FS_IS_OPEN_FLAGS_INVALID(pkt->hdr_attr, flags) ) {

		rc = SOS_ERROR_SYNTAX;  /*  Invalid flags  */
		goto error_out;
	}

	if ( flags & FS_VFS_FD_FLAG_MAY_WRITE ) {

		/*
		 * TODO: Handle READ ONLY device case
		 */
	}

	/*
	 * Create a file
	 */
	if ( flags & FS_VFS_FD_FLAG_O_CREAT ) {

		rc = fops_creat_sword(ch, fname, flags, pkt, fibp, &res);
		if ( rc != 0 ) {

			rc = res;
			goto error_out;
		}
		goto set_private_out;
	}

	/*
	 * convert the filename which was inputted from the console to
	 * the sword filename format.
	 */
	rc = fs_unix2sword(fname, &swd_name[0], SOS_FNAME_LEN);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search file from directory entry.
	 */
	rc = search_dent_sword(ch, &swd_name[0], &fib);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Check file attribute
	 */
	if ( SOS_FATTR_GET_FTYPE(fib.fib_attr) != SOS_FATTR_GET_FTYPE(pkt->hdr_attr) ) {

		rc = SOS_ERROR_NOENT;  /* File attribute was not matched. */
		goto error_out;
	}

	/*
	 * Determine whether the file is read-only-file.
	 */
	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE )
	    && ( fib.fib_attr & SOS_FATTR_RDONLY ) ) {

			rc = SOS_ERROR_RDONLY;  /* Permission denied */
			goto error_out;
	}

	/*
	 * return file information block
	 */
	if ( fibp != NULL )
		memcpy(fibp, &fib, sizeof(struct _storage_fib));

	rc = 0;

set_private_out:
	if ( privatep != NULL )
		*privatep = NULL;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Close the file
    @param[in] fdp  The file descriptor to close
    @param[out] resp     The address to store the return code for S-OS.
    @retval      0  Success
 */
int
fops_close_sword(struct _sword_file_descriptor *fdp, BYTE *resp){

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}
/** Read from the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] dest   The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] rdsizp  The adress to store read bytes.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0                Success
    @retval    -1                Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fops_read_sword(struct _sword_file_descriptor *fdp, void *dest, size_t count,
    size_t *rdsizp, BYTE *resp){
	int                        rc;
	struct _storage_disk_pos *pos;
	void                      *dp;
	size_t                remains;
	fs_off_t                  off;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	pos = &fdp->fd_pos;

	for(dp = dest, off = pos->dp_pos, remains = count; remains > 0; ) {

		rc = get_block_sword(pos->dp_devltr, &fdp->fd_fib, off,
		    FS_SWD_GTBLK_RD_FLG, &clsbuf[0], SOS_CLUSTER_SIZE, NULL);
		if ( rc != 0 )
			goto error_out;

		/*
		 * Copy data
		 */
		if ( remains > SOS_CLUSTER_SIZE ) {

			/* read and copy the record */
			memcpy(dp, &clsbuf[0], SOS_CLUSTER_SIZE);
			off += SOS_CLUSTER_SIZE;
			dp += SOS_CLUSTER_SIZE;
			remains -= SOS_CLUSTER_SIZE;
		} else {

			/* read the record and copy the remaining bytes. */
			memcpy(dp, &clsbuf[0], remains);
			off += remains;
			dp += remains;
			remains = 0;
		}
	}

	rc = 0;

error_out:
	if ( rdsizp != NULL )
		*rdsizp = count - remains;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}

/** Write to the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] src    The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] wrsizp The adress to store written bytes.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
    * SOS_ERROR_NOSPC  Device full
 */
int
fops_write_sword(struct _sword_file_descriptor *fdp, const void *src,
    size_t count, size_t *wrsizp, BYTE *resp){
	int                        rc;
	fs_off_t                  off;
	size_t                remains;
	size_t                  wrcnt;
	const void                *sp;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	pos = &fdp->fd_pos;
	fib = &fdp->fd_fib;

	/* Adjust write size */
	wrcnt = SOS_MIN(pos->dp_pos + count, SOS_MAX_FILE_SIZE );
	for(sp = src, off = pos->dp_pos, remains = wrcnt; remains > 0; ) {

		/*
		 * Write data to a block
		 */
		if ( SOS_CLUSTER_SIZE > remains ) {

			/* Write the whole record. */
			rc = put_block_sword(pos->dp_devltr,
			    fib, off, sp, SOS_CLUSTER_SIZE);
			if ( rc != 0 )
				goto update_fib;

			sp += SOS_CLUSTER_SIZE;
			off += SOS_CLUSTER_SIZE;
			remains -= SOS_CLUSTER_SIZE;
		} else {

			/* Read the record and write the remaining
			 * bytes from the beginning of the cluster.
			 */

			/* Read the contents of the last cluster. */
			rc = get_block_sword(pos->dp_devltr,
			    fib, off, FS_SWD_GTBLK_WR_FLG,
			    &clsbuf[0], SOS_CLUSTER_SIZE, NULL);
			if ( rc != 0 )
				goto update_fib;

			memcpy(&clsbuf[0], sp, remains);

			/* Write the remaining data from
			 * the beginning of the cluster.
			 */
			rc = put_block_sword(pos->dp_devltr, fib, off,
			    &clsbuf[0], SOS_CLUSTER_SIZE);
			if ( rc != 0 )
				goto update_fib;

			sp += remains;
			off += remains;
			remains = 0;
		}
	}

	rc = 0;

update_fib:
	/*
	 * Update file information block
	 */
	sos_assert( SOS_MAX_FILE_SIZE >= ( pos->dp_pos + wrcnt - remains ) );
	sos_assert( ( pos->dp_pos + wrcnt - remains ) > 0 );

	fib->fib_size = pos->dp_pos + wrcnt - remains;

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, fib);
	if ( rc != 0 )
		goto error_out;

error_out:

	if ( wrsizp != NULL )
		*wrsizp = count - remains;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}

/** Stat the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] fib    The buffer to store the file information block of the file.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
    * SOS_ERROR_NOSPC  Device full
 */
int
fops_stat_sword(struct _sword_file_descriptor *fdp, struct _storage_fib *fib,
    BYTE *resp){

	/* Copy the file infomation block */
	memmove(fib, &fdp->fd_fib, sizeof(struct _storage_fib));

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Reposition read/write file offset
    @param[in]  fdp     The file descriptor to the file.
    @param[in]  offset  The offset to reposition according to WHENCE
    excluding the size of the S-OS header.
    @param[in]  whence  The directive to reposition:
     FS_VFS_SEEK_SET The file offset is set to offset bytes.
     FS_VFS_SEEK_CUR The file offset is set to its current location plus offset bytes.
     FS_VFS_SEEK_END The file offset is set to the size of the file plus offset bytes.
    @param[out] new_posp The address to store the new position excluding the size
    of the S-OS header.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0                Success
    @retval    -EINVAL           Invalid whence
 */
int
fops_seek_sword(struct _sword_file_descriptor *fdp, fs_off_t offset, int whence,
    fs_off_t *new_posp, BYTE *resp){
	fs_off_t                  new;
	fs_off_t                  cur;
	fs_off_t                  off;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;

	pos = &fdp->fd_pos;  /* Position information */
	fib = &fdp->fd_fib;  /* File information block */

	/* Adjust offset according to the max file size */
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

		if ( resp != NULL )
			*resp = SOS_ERROR_SYNTAX;  /* return code */
		return -EINVAL;
	}

	if ( 0 > ( cur + off ) )
		new = 0;
	else if ( off > ( SOS_MAX_FILE_SIZE - cur ) )
		new = SOS_MAX_FILE_SIZE;
	else
		new = cur + off;

	if ( new_posp != NULL )
		*new_posp = new;  /* return the new position */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Truncate a file to a specified length
    @param[in]  fdp    The file descriptor to the file.
    @param[in]  offset The file length of the file to be truncated.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fops_truncate_sword(struct _sword_file_descriptor *fdp, fs_off_t offset,
    BYTE *resp){
	int                        rc;
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	fib = &fdp->fd_fib;  /* file information block */
	pos = &fdp->fd_pos;  /* position information for dirps/fatpos  */

	rc = change_filesize(fib, pos, offset);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}

/** Open directory
    @param[out] dir     The pointer to the DIR structure (directory stream).
    @param[out] resp    The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -1      Error
    @retval     EINVAL  Invalid whence
    @retval     ENXIO   The new position exceeded the file size
    @remark     DIRP has been initialized by the caller and this function is
    responsible for setting the dir_pos member of the dir_pos structured
    variable in DIR  and the private information.
 */
int
fops_opendir_sword(struct _sword_dir *dir, BYTE *resp){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	pos->dp_pos = 0;  /* Set to the first directory entry */
	pos->dp_private = NULL; /* Init private information */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Read the directory
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[out] fib    The pointer to the file information block.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
    @retval    -1      Error
    @retval     SOS_ERROR_IO    I/O Error
    @retval     SOS_ERROR_NOENT File not found (the end of the directory entry table )
    @remark     This function is responsible for setting the dir_pos member of
    the dir_pos structured variable in DIR and filling the FIB.
    Other members in the dir_pos should be set by the caller.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
    The function returns the file information block at the current position
    indicated by the dir_pos member of the dir_pos structured variable in DIR.
 */
int
fops_readdir_sword(struct _sword_dir *dir, struct _storage_fib *fib, BYTE *resp){
	int                        rc;
	struct _storage_disk_pos *pos;
	BYTE                    dirno;
	BYTE                      rec;
	BYTE    dent[SOS_DENTRY_SIZE];

	pos = &dir->dir_pos;  /* Position information */

	/*
	 * read current entry
	 */
	dirno = pos->dp_pos / SOS_DENTRY_SIZE;  /* Set #DIRNO up */

	rc = read_dent_sword(pos->dp_devltr, dirno, &rec,
	    &dent[0], SOS_DENTRY_SIZE);
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

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}

/** Set the position of the next fs_readdir() call in the directory stream.
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[in]  dirno  The position of the next fs_readdir() call
    It should be a value returned by a previous call to fs_telldir.
    @param[out] resp   The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -EINVAL Invalid dirno
    @retval     -ENXIO  The new position exceeded the SOS_DENTRY_NR.
    @remark     This function is responsible for setting the dir_pos member of
    the dir_pos structured variable in DIR and filling the FIB.
    Other members in the dir_pos should be set by the caller.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
 */
int
fops_seekdir_sword(struct _sword_dir *dir, fs_dirno dirno, BYTE *resp){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	if ( 0 > dirno )
		return -EINVAL;  /* Invalid #DIRNO */

	if ( dirno > SOS_DENTRY_NR )
		return -ENXIO;   /* #DIRNO is out of range. */

	pos->dp_pos = dirno * SOS_DENTRY_SIZE;  /* set seek position */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Return current location in directory stream
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[in]  dirno  The position of the next fs_readdir() call
    @param[out] dirnop The address to store current location in directory stream.
    It should be a value returned by a previous call to fs_telldir.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
    @retval    -1      Error
    The responses from the function:
    * SOS_ERROR_INVAL  The current position does not point the position in the file.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
 */
int
fops_telldir_sword(const struct _sword_dir *dir, fs_dirno *dirnop, BYTE *resp){
	const struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	if ( dirnop == NULL )
		goto error_out;

	*dirnop = pos->dp_pos / SOS_DENTRY_SIZE;  /* current position */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ERROR_INVAL;  /* return code */

	return -1;
}

/** close the directory
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
 */
int
fops_closedir_sword(struct _sword_dir *dir, BYTE *resp){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	pos->dp_pos = 0;        /* Reset position */
	pos->dp_private = NULL; /* Clear private information */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Change the name of a file
    @param[in]  dir      The pointer to the DIR structure (directory stream).
    @param[in]  oldpath  The filename to be changed
    @param[in]  newpath  The filename to change to.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
    The responses from the function:
    * SOS_ERROR_EXIST Newpath already exists
    * SOS_ERROR_NOENT Oldpath is not Found.
 */
int
fops_rename_sword(struct _sword_dir *dir, const unsigned char *oldpath,
    const unsigned char *newpath, BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib        old_fib;
	struct _storage_fib        new_fib;
	BYTE    old_swdname[SOS_FNAME_LEN];
	BYTE    new_swdname[SOS_FNAME_LEN];

	pos = &dir->dir_pos;  /* Position information */

	/* Get the filename of oldpath in SWORD representation. */
	rc = fs_unix2sword(oldpath, &old_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to be renamed. */
	rc = search_dent_sword(pos->dp_devltr, &old_swdname[0], &old_fib);
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
	rc = search_dent_sword(pos->dp_devltr, &new_swdname[0], &new_fib);
	if ( rc == 0 ) {

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/* Change the file name */
	memcpy(&old_fib.fib_sword_name[0],&new_swdname[0],SOS_FNAME_LEN);

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, &old_fib);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}

/** Change permission of a file
    @param[in]  dir     The pointer to the DIR structure (directory stream).
    @param[in]  path    The filename of changing the permission of the file
    @param[in]  perm    The new permission
    @param[out] resp    The address to store the return code for S-OS.
    @retval     0       Success
    @retval    -1       Error
    The responses from the function:
    * SOS_ERROR_NOENT path is not Found.
 */
int
fops_chmod_sword(struct _sword_dir *dir, const unsigned char *path,
    const fs_perm perm, BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib            fib;
	BYTE        swdname[SOS_FNAME_LEN];

	pos = &dir->dir_pos;  /* Position information */

	/* Get the filename of oldpath in SWORD representation. */
	rc = fs_unix2sword(path, &swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to be renamed. */
	rc = search_dent_sword(pos->dp_devltr, &swdname[0], &fib);
	if ( rc != 0 )
		goto error_out;

	/* Change the file permission */
	if ( perm & FS_PERM_WR )
		fib.fib_attr &= ~SOS_FATTR_RDONLY;  /* clear readonly bit */
	else
		fib.fib_attr |= SOS_FATTR_RDONLY;  /* set readonly bit */

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, &fib);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}

/** Unlink a file
    @param[in]  dir  The pointer to the DIR structure (directory stream).
    @param[in]  path The filename to unlink
    @param[out] resp The address to store the return code for S-OS.
    @retval     0    Success
    @retval    -1    Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fops_unlink_sword(struct _sword_dir *dir, const unsigned char *path,
    BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib            fib;
	BYTE        swdname[SOS_FNAME_LEN];

	pos = &dir->dir_pos;  /* Position information */

	/* Get the filename of path in SWORD representation. */
	rc = fs_unix2sword(path, &swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to unlink. */
	rc = search_dent_sword(pos->dp_devltr, &swdname[0], &fib);
	if ( rc != 0 )
		goto error_out;

	/* Change the file attribute to free */
	fib.fib_attr = SOS_FATTR_FREE;

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, &fib);
	if ( rc != 0 )
		goto error_out;

	/* Release the file allocation table for the file excluding S-OS header.
	 * @remark The file might have a bad allocation table.
	 * We should free the file allocation table after modifying the directory entry
	 * because we should make the file invisible in such a situation.
	 */
	rc = change_filesize(&fib, &dir->dir_pos, 0);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}
