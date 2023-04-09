#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sim-type.h"
#include "misc.h"
#include "fs-vfs.h"
#include "storage.h"
#include "fs-sword.h"
#include "tst-swd-swdfs.h"

/** Read/Write data to the cluster
    @param[in]  ch      The drive letter
    @param[in]  mode     The number to specify the behavior.
    * FS_VFS_IO_DIR_RD Get block to read
    * FS_VFS_IO_DIR_WR Get block to write
    @param[out] buf     The address to store the contents of the cluster.
    @param[in]  blk     The cluster number to read from or write to.
    @retval     0             success
    @retval     SOS_ERROR_IO  I/O Error
 */
static int
rw_cluster_sword(sos_devltr ch, int mode, void *buf, fs_blk_num blk){
	int                        rc;
	size_t                  rwcnt;
	BYTE blkbuf[SOS_CLUSTER_SIZE];

	/* Prepare data to write */
	if ( FS_VFS_IODIR_WRITE(mode) )
		memcpy(&blkbuf[0], buf, SOS_CLUSTER_SIZE);

	/*
	 * Read/Write a cluster.
	 */
	if ( !FS_VFS_IODIR_WRITE(mode) ) /* Read the contents of a cluster */
		rc = storage_record_read(ch, &blkbuf[0],
		    SOS_CLS2REC( SOS_CLS_VAL(blk) ), SOS_CLUSTER_RECS, &rwcnt);
	else    /* Write the contents of a buffer to the cluster. */
		rc = storage_record_write(ch, &blkbuf[0],
		    SOS_CLS2REC( SOS_CLS_VAL(blk) ), SOS_CLUSTER_RECS, &rwcnt);

	if ( rc != 0 )
		goto error_out;  /* Error */

	if ( rwcnt != SOS_CLUSTER_RECS ) {

		rc = SOS_ERROR_IO;
		goto error_out;  /* I/O Error */
	}

	/* copy data from the local buffer to BUF */
	if ( !FS_VFS_IODIR_WRITE(mode) )
		memcpy(buf, &blkbuf[0], SOS_CLUSTER_SIZE);

	return 0;

error_out:
	return rc;
}

/** Read/Write the block in the file.
    @param[in]  fib      The file information block of the file contains the block
    @param[in]  pos      The file position where the block is placed at
    @param[in]  mode     The number to specify the behavior.
    * FS_VFS_IO_DIR_RD Get block to read
    * FS_VFS_IO_DIR_WR Get block to write
    @param[out] buf       The address of the buffer to transfer the contents of
    the block
    @param[in]  bufsiz   The size of the buffer to store the contents of the block
    @param[out] rwsizp   The address to store the read or written size.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
rw_block_sword(struct _storage_fib *fib, fs_off_t pos, int mode, void *buf,
    size_t bufsiz, size_t *rwsizp){
	int                        rc;
	fs_blk_num                blk;
	size_t                 cpylen;
	size_t                remains;
	size_t                 blklen;
	fs_off_t              pos_off;
	fs_off_t              blk_off;
	BYTE blkbuf[SOS_CLUSTER_SIZE];

	pos_off = 0;                       /* offset from current file position */
	blk_off = pos % SOS_CLUSTER_SIZE;  /* copy offset in the cluster */
	remains = bufsiz;                  /* remaining bytes of request size */

	while( remains > 0 ) {

		/* Get the block number and allocate a block if it is needed. */
		rc = fs_swd_get_block_number(fib, pos + pos_off, mode, &blk);
		if ( rc != 0 ) {

			if ( ( !FS_VFS_IODIR_WRITE(mode) ) && ( rc == SOS_ERROR_NOENT ) )
				rc = 0;  /* End of file */

			goto error_out;
		}

		/* Get block length */
		rc = fs_swd_get_used_size_in_block(fib, pos + pos_off, &blklen);
		if ( rc != 0 )
			goto error_out;

		/*
		 * If the size of the block is smaller than current position,
		 * return the end of file.
		 */
		if ( ( (pos + pos_off) % SOS_CLUSTER_SIZE ) >= blklen ) {

			sos_assert( !FS_VFS_IODIR_WRITE(mode) );
			rc = 0;  /* End of file */
			goto error_out;
		}

		/* Read block into local buffer */
		rc = rw_cluster_sword(fib->fib_devltr, FS_VFS_IO_DIR_RD,
		    &blkbuf[0], blk);
		if ( rc != 0 )
			goto error_out;

		/* copy length from the buffer */
		cpylen = SOS_MIN( SOS_CLUSTER_SIZE - blk_off, remains);

		/* Adjust copy length according to the block length */
		cpylen = SOS_MIN( cpylen, blklen );

		/* copy from the buffer for the contents of the cluster */
		if ( !FS_VFS_IODIR_WRITE(mode) )  /* Read */
			memcpy(buf + pos_off, &blkbuf[0] + blk_off, cpylen);
		else { /* Write */

			/* Modify the block */
			memcpy(&blkbuf[0] + blk_off, buf + pos_off, cpylen);

			/* Write block */
			rc = rw_cluster_sword(fib->fib_devltr, FS_VFS_IO_DIR_WR,
			    &blkbuf[0], blk);
			if ( rc != 0 )
				goto error_out;
		}

		/* update positions */
		pos_off += cpylen;
		remains -= cpylen;
		blk_off = ( blk_off + cpylen ) % SOS_CLUSTER_SIZE;

		if ( ( !FS_VFS_IODIR_WRITE(mode) ) && ( SOS_CLUSTER_SIZE > blklen ) )
			break;  /* No more records */
	}

	rc = 0;

error_out:
	if ( rwsizp != NULL )
		*rwsizp = bufsiz - remains;

	return rc;
}

/** Read/Write the block in the file.
    @param[in]  fib      The file information block of the file contains the block
    @param[in]  pos      The file position where the block is placed at
    @param[in]  mode     The number to specify the behavior.
    * FS_VFS_IO_DIR_RD Get block to read
    * FS_VFS_IO_DIR_WR Get block to write
    @param[out] buf       The address of the buffer to transfer the contents of
    the block
    @param[in]  bufsiz   The size of the buffer to store the contents of the block
    @param[out] rwsizp   The address to store the read or written size.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fs_swd_read_block(struct _storage_fib *fib, fs_off_t pos, BYTE *buf,
    size_t bufsiz, size_t *rwsizp){

	return rw_block_sword(fib, pos, FS_VFS_IO_DIR_RD, (void *)buf, bufsiz, rwsizp);
}

/** Write the block to the file.
    @param[in]  fib      The file information block of the file contains the block
    @param[in]  pos      The file position where the block is placed at
    @param[in]  buf      The address of the buffer to transfer the contents of
    the block
    @param[in]  bufsiz   The size of the buffer to store the contents of the block
    @param[out] rwsizp   The address to store the read or written size.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
int
fs_swd_write_block(struct _storage_fib *fib, fs_off_t pos, const BYTE *buf,
    size_t bufsiz, size_t *rwsizp){

	return rw_block_sword(fib, pos, FS_VFS_IO_DIR_WR, (void *)buf, bufsiz, rwsizp);
}
