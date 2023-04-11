/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator - virtual file system v-node operations            */
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
#include "list.h"
#include "queue.h"

static struct _fs_vnode vnode_tbl[FS_VFS_VNODE_NR];  /* v-node table */
static struct _fs_mount mount_tbl[SOS_DEVICES_NR];   /* mount table  */

static void
clear_vnode(struct _fs_vnode *vn){

	list_init(&vn->vn_node);
	vn->vn_id = 0;
	vn->vn_status = 0;
	vn->vn_use_cnt = 0;
	vn->vn_mnt = NULL;
	storage_init_fib(&vn->vn_fib);
	vn->vn_vnode = NULL;
	vn->vn_private = NULL;
}

static int
lookup_vnode(struct _fs_mount *mnt, vfs_vnid vnid, struct _fs_vnode **vnodep){
	int               rc;
	int                i;
	struct _fs_vnode *vn;

	for(i = 0; FS_VFS_VNODE_NR > i; ++i){

		vn = &vnode_tbl[i];
		if ( ( vn->vn_id == vnid ) && ( vn->vn_mnt == mnt )
			&& !FS_VFS_IS_VNODE_FREE(vn) )
			goto found;
	}

	return ENOENT;

found:
	if ( vnodep != NULL )
		*vnodep = vn;

	return 0;
}

int
vfs_vnode_get_free_vnode(struct _fs_vnode **vnodep){
	int                         rc;
	int                          i;
	struct _fs_vnode           *vn;
	struct _fs_vnode *vn_candidate;

	for(i = 0, vn_candidate = NULL; FS_VFS_VNODE_NR > i; ++i){

		vn = &vnode_tbl[i];
		if ( FS_VFS_IS_VNODE_FREE(vn) )
			goto found;
		if ( ( vn_candidate == NULL )
		    && ( vn->vn_use_cnt == 0 )
		    && FS_VFS_IS_VNODE_BUSY( vn ) )
			vn_candidate = vn;
	}

	if ( vn_candidate == NULL )
		return ENOSPC;

	/*
	 * Invalidate v-node to obtain a free entry
	 */
	rc = vfs_invalidate_vnode(vn_candidate);
	if ( rc != 0 )
		return ENOSPC;

	vn = vn_candidate;

found:
	if ( vnodep != NULL ) {

		FS_VFS_LOCK_VNODE(vn); /* Mark busy */
		*vnodep = vn;
	}

	return 0;
}

int
vfs_put_vnode(struct _fs_vnode *vn){

	sos_assert( FS_VFS_IS_VNODE_BUSY(vn) );

	FS_VFS_UNLOCK_VNODE_(vn);

	return 0;
}

int
vfs_invalidate_vnode(struct _fs_vnode *vn){

	sos_assert( FS_VFS_IS_VNODE_BUSY(vn) );
	sos_assert( vn->vn_use_cnt == 0 );
	sos_assert( vn->vn_mnt != NULL );

	queue_del(&vn->vn_mnt->m_vnodes, &vn->vn_node);

	clear_vnode(vn);

	return 0;
}

/** lookup v-node in the v-node table
    @param[in]   ch        The drive letter
    @param[in]   ioctx     The current I/O context.
    @param[in]   vnid      The v-node ID
    @param[out]  vnodep    The address of a pointer variable to point a found v-node.
    @retval     0               Success
    @retval     SOS_ERROR_BADF  The drive is not a disk device.
    @retval     SOS_ERROR_IO    I/O Error
 */
int
fs_vfs_get_vnode(sos_devltr ch, const struct _fs_ioctx *ioctx,
    vfs_vnid vnid, struct _fs_vnode **vnodep){
	int                rc;
	int               idx;
	struct _fs_mount *mnt;
	struct _fs_vnode  *vn;

	if ( !STORAGE_DEVLTR_IS_DISK(ch) )
		return SOS_ERROR_BADF;

	/*
	 * FIXME: 以下のmountポイントからvnを検索してvnを返却するまでの
	 * 処理はmount処理部に持って行くこと
	 */
	mnt = &mount_tbl[STORAGE_DEVLTR2IDX(ch)];
	rc = lookup_vnode(mnt, vnid, &vn);
	if ( rc == 0 )
		goto found;

	if ( !FS_FSMGR_FOP_IS_DEFINED(mnt->m_fs, fops_lookup) ) {

		rc = SOS_ERROR_INVAL;
		goto error_out;
	}

	rc = vfs_vnode_get_free_vnode(&vn);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOSPC;
		goto error_out;
	}

	/* Fill v-node */
	rc = mnt->m_fs->fsm_fops->fops_lookup(ioctx, mnt->m_super, vnid, vn);
	if ( rc != 0 )
		goto error_out;

	vn->vn_mnt = mnt;
	queue_add(&mnt->m_vnodes, &vn->vn_node);

	/*
	 * FIXME: mount処理部に移動する範囲終わり
	 */

found:
	if ( vnodep != NULL )
		*vnodep = vn;

	return 0;

error_out:
	return rc;
}

/** Initialize v-node table
 */
void
fs_vfs_init_vnode_tbl(void){
	int                i;
	struct _fs_vnode *vn;

	for(i = 0; FS_VFS_VNODE_NR > i; ++i) {

		/*
		 * Init v-nodes
		 */
		vn = &vnode_tbl[i];
		clear_vnode(vn);
	}
}
