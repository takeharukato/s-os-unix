/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator sword file system module - File Allocation Table   */
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
#include "storage.h"
#include "sos.h"
#include "fs-vfs.h"
#include "fs-sword.h"

/** Read/Write file allocation table (FAT)
    @param[in]  ch  The device letter of the device
    @param[out] fatbuf Memory buffer for the FAT
    @param[in]  mode   The number to specify the behavior.
    * FS_VFS_IO_DIR_RD Get block to read
    * FS_VFS_IO_DIR_WR Get block to write
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
 */
static int
rw_fat_sword(sos_devltr ch, struct _fs_sword_fat *fat, int mode){
	int                 rc;
	size_t           rwcnt;
	fs_fatpos       fatrec;

	rc = storage_get_fatpos(ch, &fatrec);
	if ( rc != 0 )
		goto error_out;
	fatrec = SOS_FATPOS_VAL(fatrec);

	/*
	 * Read/Write the file allocation table
	 */
	if ( mode & FS_VFS_IO_DIR_WR )
		rc = storage_record_write(ch, FS_SWD_REF_FAT_TBL(fat),
		    fatrec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &rwcnt);
	else
		rc = storage_record_read(ch, FS_SWD_REF_FAT_TBL(fat),
		    fatrec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &rwcnt);

	if ( rc != 0 )
		return rc;

	if ( rwcnt != SOS_FAT_SIZE/SOS_RECORD_SIZE )
		return SOS_ERROR_IO;

	return 0;

error_out:
	return rc;
}

/** Read File Allocation Table
    @param[in]  ch     The device letter of the device
    @param[in]  fat    The address to store the read file allocation table into.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
 */
static int
read_fat_sword(sos_devltr ch, struct _fs_sword_fat *fat){

	return rw_fat_sword(ch, fat, FS_VFS_IO_DIR_RD);
}

/** Write File Allocation Table
    @param[in]  ch     The device letter of the device
    @param[in]  fat    The file allocation table to write.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
 */
static int
write_fat_sword(sos_devltr ch, const struct _fs_sword_fat *fat){

	return rw_fat_sword(ch, (struct _fs_sword_fat *)fat, FS_VFS_IO_DIR_WR);
}

/** Clear a file block
    @param[in]  fib      The file information block of the file contains the block.
    @param[in]  blkno    The block number
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
 */
static int
clear_block_sword(struct _storage_fib *fib, fs_cls blkno){
	int                     rc;
	fs_rec                 rec;
	size_t               rwcnt;
	size_t             remains;
	BYTE  buf[SOS_RECORD_SIZE];

	memset(&buf[0], 0x0, SOS_RECORD_SIZE); /* clear data */
	for(rec = SOS_CLS2REC(blkno), remains = SOS_CLUSTER_RECS;
	    remains > 0; ++rec, --remains) {

		/*
		 * clear records in the cluster
		 */
		rc = storage_record_write(fib->fib_devltr, &buf[0], rec, 1, &rwcnt);
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
    @param[in]  fib      The file information block of the file contains the block.
    @param[in]  fat      The pointer to the file allocation table cache.
    @param[in]  pos      The file offset position
    @param[in]  use_recs The used record numbers at the last cluster
    @param[out] blknop   The address to store the block number of the new block.
    @retval    0                Success
    @retval    SOS_ERROR_NOSPC  Device full
    @retval    SOS_ERROR_IO     I/O Error
  */
static int
alloc_newblock_sword(struct _storage_fib *fib, struct _fs_sword_fat *fat, fs_blk_num *blkp){
	int  i;
	int rc;

	for( i = SOS_RESERVED_FAT_NR; SOS_MAX_FILE_CLUSTER >= i; ++i)
		if ( FS_SWD_GET_FAT(fat, i) == SOS_FAT_ENT_FREE )
			goto found;  /* A free entry was found. */

	return SOS_ERROR_NOSPC;  /* Device full */

found:
	rc = clear_block_sword(fib, SOS_CLS_VAL(i));  /* clear the new block */
	if ( rc != 0 )
		return rc;

	if ( blkp != NULL )
		*blkp = i;

	return 0;
}

/** Set the number of used records at the last cluster in the cluster chain.
    @param[in]  fat    The pointer to the file allocation table cache.
    @param[in]  pos    The file offset position.
    @param[in]  blk    The block number of the last cluster.
    @retval    0                Success
    @retval    SOS_ERROR_NOSPC  Device full
  */
static void
handle_last_cluster(struct _fs_sword_fat *fat, fs_off_t pos, fs_blk_num blk){
	size_t       use_cls_siz;

	/*
	 * Calculate the number of the used records.
	 */

	/* It should write at least one byte
	 * when a new block is allocated.
	 * @remark Note that it should calculate the container size of
	 * written data, not the position to write in
	 * the following code.
	 */
	use_cls_siz = SOS_CALC_NEXT_ALIGN( ( pos % SOS_CLUSTER_SIZE ) + 1,
	    SOS_RECORD_SIZE) - 1;

	/* Write the number of used records. */
	FS_SWD_SET_FAT(fat, blk,
	    FS_SWD_CALC_FAT_ENT_AT_LAST_CLS(use_cls_siz));

	return ;
}

/** Set the number of used records at the last cluster in the cluster chain.
    @param[in]  fat    The pointer to the file allocation table cache.
    @param[in]  mode   The number to specify the behavior.
    * FS_VFS_IO_DIR_RD Get block to read
    * FS_VFS_IO_DIR_WR Get block to write
    @param[in]  blk    The block number of the last cluster.
    @param[out] fib    The file information block of the file to allocate the first block.
    @retval    0                Success
    @retval    SOS_ERROR_NOSPC  Device full
  */
static int
prepare_first_block_sword(struct _fs_sword_fat *fat, int mode, struct _storage_fib *fib){
	int              rc;
	fs_blk_num  new_blk;

	sos_assert( fib->fib_cls != SOS_FAT_ENT_FREE );

	/* When MODE is specified as FS_VFS_IO_DIR_RD
	 * and the file is empty, return SOS_ERROR_NOENT
	 * without expanding the cluster.
	 */
	if ( ( FS_SWD_IS_END_CLS(fib->fib_cls) )
	    && ( !FS_VFS_IODIR_WRITE(mode) ) ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/*
	 * Allocate the first cluster
	 */
	rc = alloc_newblock_sword(fib, fat, &new_blk);
	if ( rc != 0 )
		goto error_out;

	fib->fib_cls = new_blk;

	/* mark the end of cluster chain */
	handle_last_cluster(fat, 0, new_blk);

	return 0;

error_out:
	return rc;
}

/** Get the cluster number of the block from the file position of the file.
    @param[in]  fib      The file information block of the file contains the block.
    @param[in]  offset   The file position where the block is placed at.
    @param[in]  mode     The number to specify the behavior.
    * FS_VFS_IO_DIR_RD Get block to read
    * FS_VFS_IO_DIR_WR Get block to write
    @param[out] blkp     The address to store the cluster number in FAT.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fs_swd_get_block_number(struct _storage_fib *fib, fs_off_t offset, int mode,
    fs_blk_num *blkp){
	int                   rc;
	fs_off_t             pos;
	fs_blk_num           cur;
	fs_blk_num       blk_off;
	fs_blk_num       new_blk;
	fs_cls_off   blk_remains;
	struct _fs_sword_fat fat;

	/* Return SOS_ERROR_BADFAT when the first file allocation table entry
	 * points SOS_FAT_ENT_FREE.
	 */
	if ( fib->fib_cls == SOS_FAT_ENT_FREE )
		return SOS_ERROR_BADFAT;  /* Bad file allocation table */

	/* Read the contents of the current FAT. */
	read_fat_sword(fib->fib_devltr, &fat);

	/* If the first block has not been not alocated yet,
	 * prepare it.
	 */
	if ( FS_SWD_IS_END_CLS(fib->fib_cls) ) {

		rc = prepare_first_block_sword(&fat, mode, fib);
		if ( rc != 0 )
			goto error_out;
	}

	/* Get the first block number of the file. */
	cur = fib->fib_cls;

	/* Adjust the file potision. */
	pos = SOS_MIN(offset, SOS_MAX_FILE_SIZE);

	/* Calculate the offset block number in the file from the offset position. */
	blk_off = SOS_CALC_ALIGN(pos, SOS_CLUSTER_SIZE)	/ SOS_CLUSTER_SIZE;

	for(blk_remains = blk_off; blk_remains > 0; --blk_remains) {

		/* cur->next != NULL */
		if ( !FS_SWD_IS_END_CLS( FS_SWD_GET_FAT(&fat, cur) ) ) {

			/* If the current block is in the middle of the cluster chain,
			 * go to the next cluster.
			 */
			cur = FS_SWD_GET_FAT(&fat, cur); /* cur = cur->next */

			/* The free FAT entry should not exist in
			 * the cluster chain.
			 */
			if ( cur == SOS_FAT_ENT_FREE ) {

				rc = SOS_ERROR_BADFAT;
				goto error_out;
			}
			continue;  /* Continue searching */
		}

		if ( !FS_VFS_IODIR_WRITE(mode) ) {

			/* When MODE is specified as FS_VFS_IO_DIR_RD
			 * and the specified block is not found,
			 * return SOS_ERROR_NOENT without expanding the cluster.
			 */
			rc = SOS_ERROR_NOENT;
			goto error_out;
		}

		/*
		 * Expand the cluster chain.
		 */
		rc = alloc_newblock_sword(fib, &fat, &new_blk);
		if ( rc != 0 )
			goto error_out;

		/* First, we assume that the new block will be placed at the end of
		 * the cluster. If this assumption is incorrect, the FAT entry
		 * will be altered in the subsequent iteration.
		 */
		handle_last_cluster(&fat, pos, new_blk);

		/* Add the newly allocated block to the cluster chain. */
		FS_SWD_SET_FAT(&fat, cur, new_blk); /* cur->next = new_blk */
		cur = new_blk; /* cur = cur->next */
	}

	/* The last block might be partially allocated. */
	if ( ( FS_SWD_IS_END_CLS( FS_SWD_GET_FAT(&fat, cur) ) )
	    && ( pos % SOS_CLUSTER_SIZE
		    >= ( FS_SWD_FAT_END_CLS_RECS( FS_SWD_GET_FAT(&fat, cur) )
			* SOS_RECORD_SIZE ) ) ) {

		if ( !FS_VFS_IODIR_WRITE(mode) ) {

			rc = SOS_ERROR_NOENT; /* no record allocated at POS. */
			goto error_out;
		}

		/* Expand the cluster length */
		handle_last_cluster(&fat, pos, cur);
	}

	if ( FS_VFS_IODIR_WRITE(mode) ) { /* Write the file allocation table back. */

		rc = write_fat_sword(fib->fib_devltr, &fat);
		if ( rc != 0 )
			goto error_out;
	}

	if ( blkp != NULL )
		*blkp = cur;

	return 0;

error_out:

       /* @remark We should return without
	* writing the modified file allocation table.
	*/

       return rc;
}

/** Release blocks from the file position of the file.
    @param[in]  fib      The file information block of the file contains the block.
    @param[in]  offset   The file position where the block is placed at.
    @param[in]  relblkp  The total amount of released blocks ( unit:the number of blocks ).
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fs_swd_release_blocks(struct _storage_fib *fib, fs_off_t offset, fs_blk_num *relblkp){
	int                   rc;
	fs_off_t             pos;
	fs_off_t         rel_pos;
	fs_blk_num          next;
	fs_blk_num           cur;
	fs_blk_num  remained_blk;
	fs_blk_num      rel_blks;
	struct _fs_sword_fat fat;

	/* Return with release size 0.
	 */
	if ( fib->fib_cls == SOS_FAT_ENT_FREE )
		return SOS_ERROR_BADFAT;  /* Bad file allocation table */

	/* Adjust the file potision. */
	pos = SOS_MIN(offset, SOS_MAX_FILE_SIZE);

	/* Read the contents of the current FAT. */
	read_fat_sword(fib->fib_devltr, &fat);

	/*
	 * Release records in the last cluster
	 */
	remained_blk = SOS_FAT_ENT_UNAVAILABLE;
	if ( pos > 0 ) {

		/* Get the last block number of the remaining blocks. */
		rc = fs_swd_get_block_number(fib, pos - 1, FS_VFS_IO_DIR_RD, &remained_blk);
		if ( rc != 0 )
			goto error_out;

		/* Shrink the cluster */
		handle_last_cluster(&fat, pos - 1, remained_blk);
	}

	rel_blks = 0;   /* Initialize the number of released blocks */

	/* Start from the next cluster alignment. */
	rel_pos = SOS_CALC_NEXT_ALIGN(pos, SOS_CLUSTER_SIZE);

	/* Get the start block number to release. */
	rc = fs_swd_get_block_number(fib, rel_pos, FS_VFS_IO_DIR_RD, &next);
	if ( rc != 0 ) {

		if ( ( rc == SOS_ERROR_NOENT )
		    && ( ( pos == 0 ) || ( remained_blk != SOS_FAT_ENT_UNAVAILABLE ) ) )
			goto release_end;  /* Some records might be released. */

		goto error_out;
	}

	/*
	 * Release blocks
	 */
	do{

		cur = next; /* The block number to release. */

		if ( FS_SWD_GET_FAT(&fat, cur) == SOS_FAT_ENT_FREE ) {

			rc = SOS_ERROR_BADFAT;  /* Bad file allocation table */
			goto error_out;
		}

		next = FS_SWD_GET_FAT(&fat, cur);  /* Remember the next cluster */
		FS_SWD_SET_FAT(&fat, cur, SOS_FAT_ENT_FREE); /* Free Cluster */
		++rel_blks;   /* Increment the number of released blocks  */

	} while( !FS_SWD_IS_END_CLS( next ) );

release_end:
	/* Write the file allocation table back
	 * when some records were released.
	 */
	rc = write_fat_sword(fib->fib_devltr, &fat);
	if ( rc != 0 )
		goto error_out;

	/* Release the first block of the file */
	if ( ( !FS_SWD_IS_END_CLS( fib->fib_cls ) )
	    && ( FS_SWD_GET_FAT(&fat, fib->fib_cls) == SOS_FAT_ENT_FREE ) )
		fib->fib_cls = FS_SWD_CALC_FAT_ENT_AT_LAST_CLS(1);

	if ( relblkp != NULL )
		*relblkp = rel_blks;

	return 0;

error_out:
       /* @remark We should return without
	* writing the modified file allocation table.
	*/

	return rc;
}

/** Get used size in the block
    @param[in]  fib      The file information block of the file contains the block.
    @param[in]  offset   The file position where the block is placed at.
    @param[out] usedsizp Used size in the cluster.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fs_swd_get_used_size_in_block(struct _storage_fib *fib, fs_off_t offset, size_t *usedsizp){
	int                   rc;
	fs_off_t             pos;
	fs_blk_num           blk;
	size_t        used_bytes;
	struct _fs_sword_fat fat;

	/* Adjust the file potision. */
	pos = SOS_MIN(offset, SOS_MAX_FILE_SIZE);

	/* Read the contents of the current FAT. */
	read_fat_sword(fib->fib_devltr, &fat);

	/* Get the last block number of the remaining blocks. */
	rc = fs_swd_get_block_number(fib, pos, FS_VFS_IO_DIR_RD, &blk);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Get cluster size
	 */
	if ( !FS_SWD_IS_END_CLS( FS_SWD_GET_FAT(&fat, blk) ) )
		used_bytes = SOS_CLUSTER_SIZE;
	else
		used_bytes = FS_SWD_FAT_END_CLS_RECS( FS_SWD_GET_FAT(&fat, blk) ) * SOS_RECORD_SIZE;

	if ( usedsizp != NULL )
		*usedsizp = used_bytes;

	return 0;

error_out:
	return rc;
}
