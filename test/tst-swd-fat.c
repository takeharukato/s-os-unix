#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "storage.h"
#include "disk-2d.h"
#include "sim-type.h"
#include "misc.h"
#include "fs-vfs.h"
#include "fs-sword.h"

struct _fs_sword_fat{
	fs_sword_fatent fat[SOS_FAT_SIZE];  /**< File Allocation Table */
};

struct _fs_sword_fat tst_fat;

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

/* here */

#define FS_SWD_REF_FAT_TBL(_fatp)		\
	(  (fs_sword_fatent *)&( ( (struct _fs_sword_fat *)(_fatp) )->fat[0] ) )

#define FS_SWD_REF_FAT(_fatp, _idx)		\
	( FS_SWD_REF_FAT_TBL((_fatp)) + (_idx) )

#define FS_SWD_GET_FAT(_fatp, _idx)			\
	( *FS_SWD_REF_FAT( (_fatp), (_idx) ) )

#define FS_SWD_SET_FAT(_fatp, _idx, _v) do{				\
		*FS_SWD_REF_FAT( (_fatp), (_idx) ) = SOS_FAT_VAL((_v));	\
	}while(0)

/* Replace */

/** Determine whether the cluster is the cluster at the end of file.
    @param[in] _nxt_cls the next cluster number of the cluster to examine
    @retval TRUE   The cluster is placed at the end of file.
    @retval FALSE  The cluster is NOT placed at the end of file.
 */
#define FS_SWD_IS_END_CLS(_nxt_cls) ( ( (_nxt_cls) & SOS_FAT_ENT_EOF_MASK ) != 0 )

/** Calculate how many records are used in the cluster at the end of the file
    @param[in] _ent The value of the file allocation table entry at the end of the file
    @return The number of used records in the cluster at the end of the file
 */
#define FS_SWD_FAT_END_CLS_RECS(_ent) ( ( (_ent) & 0xf ) + 1 )

/** Calculate the number of records in the last cluster of the cluster chain.
    @param[in] _pos The file position at the end of file.
    @return The number of records in the last cluster (unit: the number of records).
 */
#define FS_SWD_CALC_RECS_AT_LAST_CLS(_pos)				\
	( SOS_CALC_NEXT_ALIGN( (_pos), SOS_RECORD_SIZE) / SOS_RECORD_SIZE )

/** Calculate the FAT entry value of the end of the file at _POS
    @param[in] _pos The file position at the end of file.
    @return The FAT entry value of the end of the cluster chain.
 */
#define FS_SWD_CALC_FAT_ENT_AT_LAST_CLS(_pos)	\
	( SOS_FAT_ENT_EOF_MASK | \
	    ( ( FS_SWD_CALC_RECS_AT_LAST_CLS( (_pos) )  - 1 ) & 0xf ) )

static int
read_fat_sword(sos_devltr ch, struct _fs_sword_fat *fat){

	memcpy(&fat[0], &tst_fat.fat[0], SOS_FAT_SIZE);
	return 0;
#if 0
	int                 rc;
	ssize_t          wrcnt;
	fs_sword_fatpos fatrec;

	rc = get_dirps_and_fatpos(ch, NULL, &fatrec);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Write the file allocation table
	 */
	rc = storage_record_write(ch, FS_SWD_REF_FATTBL(fat),
	    fatrec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &wrcnt);

	if ( rc != 0 )
		return rc;

	if ( wrcnt != SOS_FAT_SIZE/SOS_RECORD_SIZE )
		return SOS_ERROR_IO;

	return 0;

error_out:
	return rc;
#endif
}

static int
write_fat_sword(sos_devltr ch, const struct _fs_sword_fat *fat){

	memcpy(&tst_fat.fat[0], &fat[0], SOS_FAT_SIZE);
	return 0;
#if 0
	int                 rc;
	ssize_t          wrcnt;
	fs_sword_fatpos fatrec;

	rc = get_dirps_and_fatpos(ch, NULL, &fatrec);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Write the file allocation table
	 */
	rc = storage_record_write(ch, FS_SWD_REF_FATTBL(fat),
	    fatrec, SOS_FAT_SIZE/SOS_RECORD_SIZE, &wrcnt);

	if ( rc != 0 )
		return rc;

	if ( wrcnt != SOS_FAT_SIZE/SOS_RECORD_SIZE )
		return SOS_ERROR_IO;

	return 0;

error_out:
	return rc;
#endif
}

/** Allocate new block on the disk.
    @param[in]  fatp    The pointer to the file allocation table cache.
    @param[in]  pos     The file offset position
    @param[in]  use_recs The used record numbers at the last cluster
    @param[out] blknop  The address to store the block number of the new block.
    @retval    0                Success
    @retval    SOS_ERROR_NOSPC  Device full
  */
static int
alloc_newblock_sword(struct _fs_sword_fat *fatp, fs_blk_num *blkp){
	int  i;

	for( i = SOS_RESERVED_FAT_NR; SOS_MAX_FILE_CLUSTER >= i; ++i)
		if ( FS_SWD_GET_FAT(fatp, i) == SOS_FAT_ENT_FREE )
			goto found;  /* A free entry was found. */

	return SOS_ERROR_NOSPC;  /* Device full */

found:
	if ( blkp != NULL )
		*blkp = i;

	return 0;
}
static void
handle_last_cluster(struct _fs_sword_fat *fat, fs_off_t pos, int mode, fs_blk_num blk){
	size_t       use_cls_siz;

	if ( !FS_SWD_IS_END_CLS(FS_SWD_GET_FAT(fat, blk)) )
		return ;  /* No need to calculate the size of the used records */

	/* Calculate the number of the used records.
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

static int
prepare_first_block_sword(struct _fs_sword_fat *fat, struct _storage_fib *fib,
    int mode){
	int              rc;
	fs_blk_num  new_blk;

	sos_assert( fib->fib_cls != SOS_FAT_ENT_FREE );

	/* When MODE is specified as FS_SWD_GTBLK_RD_FLG
	 * and the file is empty, return SOS_ERROR_NOENT
	 * without expanding the cluster.
	 */
	if ( ( FS_SWD_IS_END_CLS(fib->fib_cls) )
	    && ( !FS_SWD_GETBLK_TO_WRITE(mode) ) ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/*
	 * Allocate the first cluster
	 */
	rc = alloc_newblock_sword(fat, &new_blk);
	if ( rc != 0 )
		goto error_out;

	fib->fib_cls = new_blk;

	/* mark the end of cluster chain */
	FS_SWD_SET_FAT(fat, new_blk, FS_SWD_CALC_FAT_ENT_AT_LAST_CLS(1));
	handle_last_cluster(fat, 0, mode, new_blk);

	return 0;

error_out:
	return rc;
}
/** Get the cluster number of the block from the file position of the file.
    @param[in]  fib      The file information block of the file contains the block.
    @param[in]  offset   The file position where the block is placed at.
    @param[in]  mode     The number to specify the behavior.
    * FS_SWD_GTBLK_RD_FLG Get block to read
    * FS_SWD_GTBLK_WR_FLG Get block to write
    @param[out] blkp     The address to store the cluster number in FAT.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
get_cluster_number_sword(struct _storage_fib *fib, fs_off_t offset, int mode,
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

		rc = prepare_first_block_sword(&fat, fib, mode);
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

		if ( !FS_SWD_GETBLK_TO_WRITE(mode) ) {

			/* When MODE is specified as FS_SWD_GTBLK_RD_FLG
			 * and the specified block is not found,
			 * return SOS_ERROR_NOENT without expanding the cluster.
			 */
			rc = SOS_ERROR_NOENT;
			goto error_out;
		}

		/*
		 * Expand the cluster chain.
		 */
		rc = alloc_newblock_sword(&fat, &new_blk);
		if ( rc != 0 )
			goto error_out;

		/* Assume all records are used. */
		FS_SWD_SET_FAT(&fat, new_blk,
		    FS_SWD_CALC_FAT_ENT_AT_LAST_CLS( SOS_CLUSTER_SIZE - 1 ));

		if ( blk_remains == 1 )  /* When the block is placed at the end. */
			handle_last_cluster(&fat, pos, mode, new_blk);

		/* Add the newly allocated block to the cluster chain. */
		FS_SWD_SET_FAT(&fat, cur, new_blk); /* cur->next = new_blk */
		cur = new_blk; /* cur = cur->next */
	}

	/* The last block might be partially allocated. */
	if ( ( FS_SWD_IS_END_CLS( FS_SWD_GET_FAT(&fat, cur) ) )
	    && ( pos % SOS_CLUSTER_SIZE
		    >= ( FS_SWD_FAT_END_CLS_RECS( FS_SWD_GET_FAT(&fat, cur) )
			* SOS_RECORD_SIZE ) ) ) {

		if ( !FS_SWD_GETBLK_TO_WRITE(mode) ) {

			rc = SOS_ERROR_NOENT; /* no record allocated at POS. */
			goto error_out;
		}
		/* Expand the cluster length */
		handle_last_cluster(&fat, pos, mode, cur);
	}

	if ( blkp != NULL )
		*blkp = cur;

	if ( FS_SWD_GETBLK_TO_WRITE(mode) ) /* Write the file allocation table back. */
		write_fat_sword(fib->fib_devltr, &fat);

	return 0;

error_out:

       /* @remark We should return without
	* writing the modified file allocation table.
	*/

       return rc;
}


void
reset_fat(void){

	memset(&tst_fat, 0x00, sizeof(struct _fs_sword_fat));

	FS_SWD_SET_FAT(&tst_fat,0,0x8f);
	FS_SWD_SET_FAT(&tst_fat,1,0x8f);
}

int
main(int argc, char *argv[]){
	int                  rc;
	struct _storage_fib fib;
	fs_blk_num          blk;

	/*
	 * Init FAT
	 */
	reset_fat();

	fib.fib_cls = 0x00;
	blk=0;
	rc = get_cluster_number_sword(&fib, 0, FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc == SOS_ERROR_BADFAT );

	FS_SWD_SET_FAT(&tst_fat,2,0x0);
	fib.fib_cls = 0x02;
	blk=0;
	rc = get_cluster_number_sword(&fib, SOS_CLUSTER_SIZE, FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc == SOS_ERROR_BADFAT );

	fib.fib_cls = 0x8f;
	blk=0;
	rc = get_cluster_number_sword(&fib, 0, FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc == SOS_ERROR_NOENT );

	fib.fib_cls = 0x8f;
	blk=0;
	rc = get_cluster_number_sword(&fib, 0, FS_SWD_GTBLK_WR_FLG, &blk);
	sos_assert( rc == 0 );
	sos_assert( blk == 2 );
	sos_assert( FS_SWD_GET_FAT(&tst_fat, blk) == 0x80 );

	blk=0;
	rc = get_cluster_number_sword(&fib, 0, FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc == 0 );
	sos_assert( blk == 2 );
	sos_assert( FS_SWD_GET_FAT(&tst_fat, blk) == 0x80 );

	blk=0;
	rc = get_cluster_number_sword(&fib, 255, FS_SWD_GTBLK_WR_FLG, &blk);
	sos_assert( rc == 0 );
	sos_assert( blk == 2 );
	sos_assert( FS_SWD_GET_FAT(&tst_fat, blk) == 0x80 );

	/*
	 * Get second cluster
	 */
	blk=0;
	rc = get_cluster_number_sword(&fib, SOS_CLUSTER_SIZE, FS_SWD_GTBLK_WR_FLG, &blk);
	sos_assert( rc == 0 );
	sos_assert( blk == 3 );
	sos_assert( FS_SWD_GET_FAT(&tst_fat, blk) == 0x80 );

	/*
	 * search valid block in the chain
	 */
	blk=0;
	rc = get_cluster_number_sword(&fib, SOS_CLUSTER_SIZE, FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc == 0 );
	sos_assert( blk == 3 );
	sos_assert( FS_SWD_GET_FAT(&tst_fat, blk) == 0x80 );

	blk=0;
	rc = get_cluster_number_sword(&fib, 0, FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc == 0 );
	sos_assert( blk == 2 );
	sos_assert( FS_SWD_GET_FAT(&tst_fat, blk) == 0x3 );

	/*
	 * search invalid block in the chain
	 */
	blk=0;
	rc = get_cluster_number_sword(&fib, SOS_CLUSTER_SIZE*2, FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc != 0 );
	sos_assert( rc == SOS_ERROR_NOENT );

	blk=0;
	rc = get_cluster_number_sword(&fib, SOS_CLUSTER_SIZE + SOS_RECORD_SIZE,
	    FS_SWD_GTBLK_RD_FLG, &blk);
	sos_assert( rc != 0 );
	sos_assert( rc == SOS_ERROR_NOENT );

	blk=0;
	rc = get_cluster_number_sword(&fib, SOS_CLUSTER_SIZE + SOS_RECORD_SIZE,
	    FS_SWD_GTBLK_WR_FLG, &blk);
	sos_assert( rc == 0 );
	sos_assert( blk == 3 );
	sos_assert( FS_SWD_GET_FAT(&tst_fat, blk) == 0x81 );


	return 0;
}
