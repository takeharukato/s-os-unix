/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator - virtual file system mount point operations       */
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

static struct _fs_mount mount_tbl[SOS_DEVICES_NR];   /* mount table  */

/** Release v-nodes int  the mount point
    @param[out] mnt The mount point
 */
static void
release_vnodes_from_mount_point(struct _fs_mount *mnt){
	struct _fs_vnode  *vn;
	struct _list     *itr;
	struct _list     *nxt;

	queue_for_each_safe(itr, &mnt->m_vnodes, nxt){

		vn = container_of(itr, struct _fs_vnode, vn_node);

		--vn->vn_use_cnt;
		sos_assert( vn->vn_use_cnt == 0 );

		FS_VFS_LOCK_VNODE(vn);  /* Lock a v-node to release */

		vfs_invalidate_vnode(vn); /* Release v-node */
	}

}

/** Clear the mount point information
    @param[out] mnt The mount point to clear
 */
static void
clear_mount_point(struct _fs_mount *mnt){

	mnt->m_devltr=0;
	queue_init(&mnt->m_vnodes);
	mnt->m_fs = NULL;
	mnt->m_root = NULL;
	mnt->m_super = NULL;
	mnt->m_mount_flags = 0;
}

/** Determine whether the mount point is busy.
    @param[in] mnt       The mount point to check
    @param[in] ioctx     The current I/O context.
    @retval    TRUE      The file system is busy.
    @retval    FALSE     The file system can be unmounted.
 */
static int
check_mount_point_is_busy(const struct _fs_mount *mnt,
    const struct _fs_ioctx *ioctx){
	struct _fs_vnode   *vn;
	struct _fs_vnode  *cwd;
	struct _list      *itr;

	sos_assert( mnt->m_root == ioctx->ioc_root[mnt->m_devltr] );

	cwd = ioctx->ioc_cwd[mnt->m_devltr];

	queue_for_each(itr, &mnt->m_vnodes){

		vn = container_of(itr, struct _fs_vnode, vn_node);
		if ( vn == mnt->m_root )
			continue;

		if ( vn == cwd )
			continue;

		if ( FS_VFS_IS_VNODE_BUSY(vn) )
			return TRUE;

		if ( ( vn == cwd ) && ( vn->vn_use_cnt > 1 ) )
			return TRUE;

		if ( vn->vn_use_cnt > 0 )
			return TRUE;
	}

	/*
	 * Check root v-node and current working directory
	 */
	if ( cwd == mnt->m_root ) {

		if ( mnt->m_root->vn_use_cnt > 2 )
			return TRUE;
	} else if ( ( cwd->vn_use_cnt > 1 ) || ( mnt->m_root->vn_use_cnt > 1 ) )
			return TRUE;

	return FALSE;
}

/** lookup v-node from the mount point
    @param[in]   mnt       The mount point
    @param[in]   vnid      The v-node ID
    @param[out]  vnodep    The address of a pointer variable to point a found v-node.
    @retval     0               Success
    @retval     SOS_ERROR_NOENT v-node not found.
 */
static int
mnt_lookup_vnode(const struct _fs_mount *mnt, vfs_vnid vnid, struct _fs_vnode **vnodep){
	struct _fs_vnode  *vn;
	struct _list     *itr;

	queue_for_each(itr, &mnt->m_vnodes){

		vn = container_of(itr, struct _fs_vnode, vn_node);
		if ( vn->vn_id == vnid )
			goto found;
	}

	return SOS_ERROR_NOENT;  /* Not found */

found:
	if ( vnodep != NULL )
		*vnodep = vn;

	return 0;
}

/** lookup v-node in the v-node table
    @param[in]   ch        The drive letter
    @param[in]   ioctx     The current I/O context.
    @param[in]   vnid      The v-node ID
    @param[out]  vnodep    The address of a pointer variable to point a found v-node.
    @retval     0                Success
    @retval     SOS_ERROR_BADF   The drive is not a disk device.
    @retval     SOS_ERROR_INVAL  The operation is not supported.
    @retval     SOS_ERROR_NOSPC  No more memory.
    @retval     SOS_ERROR_IO     I/O Error
    @retval     SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fs_vfs_mnt_search_vnode(sos_devltr ch, const struct _fs_ioctx *ioctx,
    vfs_vnid vnid, struct _fs_vnode **vnodep){
	int                rc;
	int               idx;
	struct _fs_mount *mnt;
	struct _fs_vnode  *vn;

	if ( !STORAGE_DEVLTR_IS_DISK(ch) )
		return SOS_ERROR_BADF;

	/*
	 * Look up the v-node from a mount table.
	 */
	idx = STORAGE_DEVLTR2IDX(ch);
	mnt = &mount_tbl[idx];
	rc = mnt_lookup_vnode(mnt, vnid, &vn);
	if ( rc == 0 )
		goto found;

	if ( !FS_FSMGR_FOP_IS_DEFINED(mnt->m_fs, fops_lookup) ) {

		rc = SOS_ERROR_INVAL;
		goto error_out;
	}

	/*
	 * Get a free v-node cache
	 */
	rc = vfs_vnode_get_free_vnode(&vn);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOSPC;
		goto error_out;
	}

	/* Fill v-node */
	rc = mnt->m_fs->fsm_fops->fops_lookup(ch, ioctx, mnt->m_super, vnid, vn);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Add the v-node into the mount point.
	 */
	vn->vn_mnt = mnt;
	queue_add(&mnt->m_vnodes, &vn->vn_node);

found:
	if ( vnodep != NULL )
		*vnodep = vn;

	return 0;

error_out:
	return rc;
}

/** Mount a file system
    @param[in]   ch        The drive letter.
    @param[in]   fs_name   The file system name.
    @param[in]   args      The mount option information.
    @param[out]  ioctx     The current I/O context.
    @retval      0         Success
    @retval      ENOENT    Neither device nor file system is invalid.
    @retval      EBUSY     Already mounted.
    @retval      EIO       Mount operation failed.
 */
int
fs_vfs_mnt_mount_filesystem(sos_devltr ch, const char *fs_name, const void *args,
    struct _fs_ioctx *ioctx){
	int                        rc;
	int                       idx;
	struct _fs_mount         *mnt;
	struct _fs_fs_manager *fs_mgr;
	vfs_fs_super            super;
	vfs_mnt_flags       mnt_flags;
	struct _fs_vnode     *root_vn;
	struct _fs_vnode          *vn;

	if ( !STORAGE_DEVLTR_IS_DISK(ch) )
		return ENOENT;

	/*
	 * Get mount point
	 */
	idx = STORAGE_DEVLTR2IDX(ch);
	mnt = &mount_tbl[idx];
	if ( mnt->m_fs != NULL )
		return EBUSY;

	/*
	 * Get the file system
	 */
	rc = fs_vfs_lookup_filesystem(fs_name, &fs_mgr);
	if ( rc != 0 )
		return ENOENT;

	if ( !FS_FSMGR_FOP_IS_DEFINED(fs_mgr, fops_mount) )
		return ENOENT;

	/*
	 * mount a file system
	 */
	rc = fs_mgr->fsm_fops->fops_mount(ch, args, ioctx,
	    &super, &mnt_flags, &root_vn);
	if ( rc != 0 )
		return EIO;

	sos_assert( root_vn->vn_use_cnt == 0 );

	/*
	 * Initialize the mount point
	 */
	mnt->m_devltr = ch;
	queue_init(&mnt->m_vnodes);
	mnt->m_fs = fs_mgr;
	mnt->m_root = root_vn;
	mnt->m_super = super;
	mnt->m_mount_flags = mnt_flags;

	/* Refer from the mount point and
	 * S-OS monitor (current working directory)
	 */
	ioctx->ioc_root[mnt->m_devltr] = mnt->m_root;
	ioctx->ioc_cwd[mnt->m_devltr] = mnt->m_root;
	root_vn->vn_use_cnt += 2;

	return 0;
}

/** Unmount a file system
    @param[in]   ch        The drive letter.
    @param[out]  ioctx     The current I/O context.
    @retval      0         Success
    @retval      ENOENT    No file system is not mounted.
 */
int
fs_vfs_mnt_unmount_filesystem(sos_devltr ch, struct _fs_ioctx *ioctx){
	int                        rc;
	int                       idx;
	struct _fs_mount         *mnt;
	struct _fs_fs_manager *fs_mgr;
	struct _fs_vnode          *vn;

	if ( !STORAGE_DEVLTR_IS_DISK(ch) )
		return ENOENT;

	/*
	 * Get mount point
	 */
	idx = STORAGE_DEVLTR2IDX(ch);
	mnt = &mount_tbl[idx];
	if ( mnt->m_fs == NULL )
		return ENOENT;

	sos_assert(mnt->m_root == ioctx->ioc_root[mnt->m_devltr] );

	if ( !FS_FSMGR_FOP_IS_DEFINED(fs_mgr, fops_unmount) )
		return ENOENT;

	/*
	 * Check whether the file system is busy.
	 */
	if ( check_mount_point_is_busy(mnt, ioctx) )
		return EBUSY;

	/*
	 * unmount a file system
	 */
	rc = fs_mgr->fsm_fops->fops_unmount(ch, mnt->m_super, mnt->m_root);
	if ( rc != 0 )
		return EIO;

	/*
	 * Clear current working directory
	 */
	vn = ioctx->ioc_cwd[mnt->m_devltr];
	sos_assert( ( vn != ioctx->ioc_root[mnt->m_devltr] ) ||
	    ( vn->vn_use_cnt == 1 ) );
	--vn->vn_use_cnt;
	ioctx->ioc_cwd[mnt->m_devltr] = NULL;

	/*
	 *  Clear root directory
	 */
	vn = ioctx->ioc_root[mnt->m_devltr];
	sos_assert( vn->vn_use_cnt == 1 );

	--vn->vn_use_cnt;
	ioctx->ioc_root[mnt->m_devltr] = NULL;

	/* Release v-nodes */
	release_vnodes_from_mount_point(mnt);

	/* Clear the mount point */
	clear_mount_point(mnt);

	return 0;
}

/** Initialize mount table
 */
void
fs_vfs_init_mount_tbl(void){
	int                 i;
	struct _fs_mount *mnt;


	for(i = 0; FS_VFS_VNODE_NR > i; ++i) {

		/*
		 * Init mount point information
		 */
		mnt = &mount_tbl[i];
		clear_mount_point(mnt);
		mnt->m_devltr=STORAGE_IDX2DRVLTR(i);
	}
}
