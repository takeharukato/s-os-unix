#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "storage.h"
#include "sim-type.h"
#include "misc.h"
#include "fs-vfs.h"
#include "fs-sword.h"
#include "tst-swd-swdfs.h"

/** Read/Write cluster
    @param[in]  ch      The drive letter
    @param[in]  mode     The number to specify the behavior.
    * FS_IO_DIR_RD Get block to read
    * FS_IO_DIR_WR Get block to write
    @param[out] buf     The address to store the contents of the cluster.
    @param[in]  blk     The cluster number to read from or write to.
    @param[in]  count   The length to read/write.
    @retval     0             success
    @retval     SOS_ERROR_IO  I/O Error
 */
static int
rw_cluster_sword(sos_devltr ch, int mode, void *buf, fs_blk_num blk, size_t count){
	int                        rc;
	size_t                  rwcnt;
	fs_rec_off             recoff;
	size_t             rw_remains;
	BYTE  recbuf[SOS_RECORD_SIZE];

	/*
	 * read/write cluster
	 */
	rw_remains = count;

	if ( buf == NULL )
		goto null_buff;

	for(recoff = 0;
	    rw_remains > 0 ;
	    rw_remains -= SOS_MIN(rw_remains, SOS_RECORD_SIZE), ++recoff) {

		if ( !FS_SWD_GETBLK_TO_WRITE(mode) ) { /* Read the contents of a cluster */

			rc = storage_record_read(ch, &recbuf[0],
			    SOS_CLS2REC( SOS_CLS_VAL(blk) ) + recoff, 1, &rwcnt);

			if ( rc != 0 )
				goto error_out;  /* Error */

			if ( rwcnt != 1 ) {

				rc = SOS_ERROR_IO;
				goto error_out;  /* I/O Error */
			}

			/*
			 * Copy contents of the block
			 */
			memcpy(buf + recoff * SOS_RECORD_SIZE,
			    &recbuf[0], SOS_MIN(rw_remains, SOS_RECORD_SIZE));
		} else {

			/*
			 * Write the data to a cluster.
			 */
			if ( SOS_RECORD_SIZE > rw_remains ) { /* Write a part of the record. */

				/*
				 * Modify the beginning of the buffer at the end of file
				 */
				rc = storage_record_read(ch, &recbuf[0],
				    SOS_CLS2REC( SOS_CLS_VAL(blk) ) + recoff, 1, &rwcnt);

				if ( rc != 0 )
					goto error_out;  /* I/O Error */

				if ( rwcnt != 1 ) {

					rc = SOS_ERROR_IO;
					goto error_out;  /* I/O Error */
				}

				memcpy(&recbuf[0],
				    buf + recoff * SOS_RECORD_SIZE,
				    rw_remains);  /* Modify */

				/*
				 * Write back
				 */
				rc = storage_record_write(ch, &recbuf[0],
				    SOS_CLS2REC( SOS_CLS_VAL(blk) ) + recoff, 1, &rwcnt);

				if ( rc != 0 )
					goto error_out;  /* I/O Error */

				if ( rwcnt != 1 ) {

					rc = SOS_ERROR_IO;
					goto error_out;  /* I/O Error */
				}

				rw_remains = 0;

			} else {  /* Write a whole record. */

				rc = storage_record_write(ch, buf + recoff * SOS_RECORD_SIZE,
				    SOS_CLS2REC( SOS_CLS_VAL(blk) ) + recoff, 1, &rwcnt);

				if ( rc != 0 )
					goto error_out;  /* I/O Error */

				if ( rwcnt != 1 ) {

					rc = SOS_ERROR_IO;
					goto error_out;  /* I/O Error */
				}
			}
		}
	}

null_buff:
	rc = 0;

error_out:
	return rc;
}

/** Read/Write the block in the file.
    @param[in]  fib      The file information block of the file contains the block
    @param[in]  pos      The file position where the block is placed at
    @param[in]  mode     The number to specify the behavior.
    * FS_IO_DIR_RD Get block to read
    * FS_IO_DIR_WR Get block to write
    @param[out] buf       The address of the buffer to transfer the contents of
    the block
    @param[in]  bufsiz   The size of the buffer to store the contents of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
rw_block_sword(struct _storage_fib *fib, fs_off_t pos, int mode, void *buf, size_t bufsiz){
	int                     rc;
	fs_blk_num             blk;

	/*
	 * Get the cluster number of POS
	 */
	rc = fs_swd_get_block_number(fib, pos, mode, &blk);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Read/Write cluster
	 */
	rc = rw_cluster_sword(fib->fib_devltr, mode, buf, blk, bufsiz);
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/** Read/Write the block in the file.
    @param[in]  fib      The file information block of the file contains the block
    @param[in]  pos      The file position where the block is placed at
    @param[in]  mode     The number to specify the behavior.
    * FS_IO_DIR_RD Get block to read
    * FS_IO_DIR_WR Get block to write
    @param[out] buf       The address of the buffer to transfer the contents of
    the block
    @param[in]  bufsiz   The size of the buffer to store the contents of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fs_swd_read_block(struct _storage_fib *fib, fs_off_t pos, BYTE *buf, size_t bufsiz){

	return rw_block_sword(fib, pos, FS_IO_DIR_RD, (void *)buf, bufsiz);
}

/** Write the block to the file.
    @param[in]  fib      The file information block of the file contains the block
    @param[in]  pos      The file position where the block is placed at
    @param[in]  buf      The address of the buffer to transfer the contents of
    the block
    @param[in]  bufsiz   The size of the buffer to store the contents of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fs_swd_write_block(struct _storage_fib *fib, fs_off_t pos, const BYTE *buf, size_t bufsiz){

	return rw_block_sword(fib, pos, FS_IO_DIR_WR, (void *)buf, bufsiz);
}