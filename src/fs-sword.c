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

static struct _fs_fops sword_fops={
	.fops_mount = fops_mount_sword,
	.fops_unmount = fops_unmount_sword,
	.fops_get_vnode = fops_get_vnode_sword,
};
static struct _fs_fs_manager sword_fsm;

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

	vnid = FS_SWD_ROOT_VNID;  /* Root v-node */
	rc = vfs_vnode_get_free_vnode(&vn);
	if ( rc != 0 )
		goto error_out;

	if ( mnt_flagsp != NULL )
		*mnt_flagsp = (vfs_mnt_flags)(uintptr_t)args;

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

/** look up v-node in the directory entry (the v-node table)
    @param[in] ch         The drive letter
    @param[in] ioctx      The current I/O context
    @param[in] super      The file system specific super block information.
    @param[in] vnid       The v-node ID to find
    @param[in] vnp        The address to store v-node
    @retval    0          Success
 */
int
fops_get_vnode_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    vfs_fs_super super, vfs_vnid vnid, struct _fs_vnode *vnp){
	int                  rc;
	struct _fs_vnode     vn;
	struct _storage_fib fib;


	vfs_vnode_init_vnode(&vn); /* Init v-node */

	if ( vnid != FS_SWD_ROOT_VNID ) {

		/* Search file information block in the directory entry */
		rc = fs_swd_search_fib_by_vnid(ch, ioctx,
		    FS_SWD_GET_VNID2DIRCLS(vnid), vnid, &fib);
		if ( rc != 0 )
			goto error_out;

		/* Fill the file information block */
		memcpy(&vn.vn_fib, &fib, sizeof(struct _storage_fib));
	}

	/*
	 * Return v-node
	 */
	vn.vn_id = vnid;
	memcpy(vnp, &vn, sizeof(struct _fs_vnode));

	return 0;

error_out:
	return rc;
}


void
init_sword_filesystem(void){

	fs_vfs_init_file_manager(&sword_fsm);
	sword_fsm.fsm_name = FS_SWD_FSNAME;
	sword_fsm.fsm_fops = &sword_fops;
	fs_vfs_register_filesystem(&sword_fsm);
}
