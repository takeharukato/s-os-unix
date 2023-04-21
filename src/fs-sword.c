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
#include "fs-vfs.h"
#include "storage.h"
#include "fs-sword.h"


/** Truncate a file to a specified length
    @param[in]  ioctx  The current I/O context
    @param[in]  fib    The file information block of the file.
    @param[in]  pos    The file position information
    @param[in]  newpos The file position of the file to be truncated.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
static int
change_filesize_sword(struct _fs_ioctx *ioctx, const struct _fs_vnode *dir_vnode,
    struct _storage_fib *fib, struct _storage_disk_pos *pos, fs_off_t newpos){
	int                        rc;
	fs_off_t               newsiz;
	fs_off_t              extends;
	fs_blk_num                blk;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	if ( ( 0 > newpos ) || ( newpos > SOS_MAX_FILE_SIZE ) )
		return SOS_ERROR_SYNTAX;

	newsiz = newpos;

	extends = newsiz - fib->fib_size;

	if ( 0 >= extends ) {

		/*
		 * Release file blocks
		 */
		rc = fs_swd_release_blocks(ioctx, fib, newsiz, NULL);
		if ( rc != 0 )
			goto error_out;
	} else {

		/* alloc new blocks to the newsize. */
		rc = fs_swd_get_block_number(ioctx, fib, newsiz, FS_VFS_IO_DIR_WR, &blk);
		if ( rc != 0 )
			goto error_out;

		/*
		 * Clear extra bytes after the end of file.
		 */
		if ( ( newsiz > 0) && ( ( ( newsiz + 1 ) % SOS_CLUSTER_SIZE ) > 0 ) ) {

			rc = fs_swd_read_block(ioctx, fib, SOS_CALC_ALIGN(newsiz,
				SOS_CLUSTER_SIZE), &clsbuf[0],
			    SOS_CLUSTER_SIZE, NULL);
			if ( rc != 0 )
				goto error_out;

			memset((void *)&clsbuf[0] + ( newsiz + 1) % SOS_CLUSTER_SIZE,
			    0x0,
			    SOS_CLUSTER_SIZE - ( ( newsiz + 1) % SOS_CLUSTER_SIZE ) );

			rc = fs_swd_write_block(ioctx, fib,
			    SOS_CALC_ALIGN(newsiz, SOS_CLUSTER_SIZE),
			    &clsbuf[0], SOS_CLUSTER_SIZE, NULL);
			if ( rc != 0 )
				goto error_out;
		}
	}

	/*
	 * update file information block
	 */
	fib->fib_size = STORAGE_FIB_FIX_SIZE( newsiz );  /* update size */

	rc = fs_swd_write_dent(fib->fib_devltr, ioctx, dir_vnode, fib); /* write back */
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/*
 * File system operations
 */

/** Mount a file system
    @param[in] ch       The drive letter
    @param[in] args     The file system specific mount option
    @param[in] ioctx     The current I/O context
    @param[out] superp    The address to store the file system specific super block information.
    @param[out] mnt_flagsp The address to store the file system specific super block information
    @param[out] root_vnodep The address of the pointer variable to point the root v-node.
    @retval     0               Success
    @retval     ENOSPC  No more space
    @retval     SOS_ERROR_BADF  The drive is not a disk device.
    @retval     SOS_ERROR_IO    I/O Error
 */
int
fops_mount_sword(sos_devltr ch, const void *args,
    struct _fs_ioctx *ioctx, vfs_fs_super *superp,
    vfs_mnt_flags *mnt_flagsp, struct _fs_vnode **root_vnodep){
	int                        rc;
	vfs_vnid                 vnid;
	struct _fs_vnode          *vn;
	struct _fs_sword_mnt_opt *opt;
	vfs_mnt_flags           flags;

	vnid = 0;  /* Root v-node */
	rc = fs_vfs_get_vnode(ch, ioctx, vnid, &vn);
	if ( rc != 0 )
		goto error_out;

	flags = 0;
	opt = (struct _fs_sword_mnt_opt *)args;

	if ( mnt_flagsp != NULL )
		*mnt_flagsp = opt->mount_opts;

	if ( superp != NULL )
		*superp = NULL;

	if ( root_vnodep != NULL )
		*root_vnodep = vn;

	return 0;

error_out:
	return rc;
}

/** Unmount file system
    @param[in] ch         The drive letter.
    @param[in] super      The file system specific super block information.
    @param[in] ioctx      The current I/O context.
    @param[in] root_vnode The address of the pointer variable to point the root v-node.
    @retval    0          Success
 */
int
fops_unmount_sword(sos_devltr ch, vfs_fs_super super,
    struct _fs_vnode *root_vnode){

	return 0;
}
