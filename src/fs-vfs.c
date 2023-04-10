/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator - virtual file system module                       */
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

/** Filesystem table */
static struct _fs_filesystem_table fs_tbl = {__QUEUE_INITIALIZER(&fs_tbl.head)};

/** Initialize the file descriptor
    @param[in]  ch    The drive letter
    @param[in]  ioctx The current I/O context.
    @param[out] fdp   The address to the file descriptor (file handler) to initialize
 */
static void
init_fd(sos_devltr ch, struct _fs_ioctx *ioctx, struct _sword_file_descriptor *fdp){
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	memset(fdp, 0x0, sizeof(struct _sword_file_descriptor));   /* just in case */

	storage_init_fib(&fdp->fd_fib);       /* Initialize the file information block */
	storage_init_position(&fdp->fd_pos);  /* Initialize position */

	fib = &fdp->fd_fib;
	pos = &fdp->fd_pos;

	fib->fib_devltr = ch;  /* Drive letter */

	storage_get_dirps(ch, &pos->dp_dirps);    /* FIXME: Set #DIRPS of the device */
	storage_get_fatpos(ch, &pos->dp_fatpos);  /* FIXME: Set #FATPOS of the device */

	/*
	 * Clear flags
	 */
	fdp->fd_flags = 0;
	fdp->fd_sysflags = 0;

	fdp->fd_ioctx = ioctx;
	fdp->fd_private=NULL;  /* Initialize private information */
}

/** Initialize the directory stream
    @param[in]  ch    The drive letter
    @param[in]  ioctx The current I/O context.
    @param[out] dir   The address to the directory stream to initialize
 */
static void
init_dir_stream(sos_devltr ch, struct _fs_ioctx *ioctx, struct _sword_dir *dir){
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	storage_init_fib(&dir->dir_fib);      /* Initialize the file information block */
	storage_init_position(&dir->dir_pos); /* Initialize position */

	fib = &dir->dir_fib;
	pos = &dir->dir_pos;

	fib->fib_devltr = ch;  /* Drive letter */

	storage_get_dirps(ch, &pos->dp_dirps);    /* FIXME: DIRPS  */
	storage_get_fatpos(ch, &pos->dp_fatpos);  /* FIXME: FATPOS */

	/*
	 * Clear flags
	 */
	dir->dir_sysflags = 0;
	dir->dir_private = NULL;

	dir->dir_ioctx = ioctx;
}

/** Initialize the directory stream
    @param[in]  ioctx  The current I/O context.
 */
void
fs_vfs_init_ioctx(struct _fs_ioctx *ioctx){
	int                    i;

	for( i = 0; STORAGE_NR > i; ++i) {

		/* Initialize root fib and current working directory */
		storage_init_fib(&ioctx->ioc_root[i]);
		memcpy(&ioctx->ioc_cwd[i], &ioctx->ioc_root[i],
		    sizeof(struct _storage_fib));
	}

	for( i = 0; FS_PROC_FDTBL_NR > i; ++i)
		init_fd(0, ioctx, &ioctx->ioc_fds[i]);
}

/** Register a file system
    @param[in] fsm_ops  The pointer to the file system manager to register
    @retval    0     success
    @retval    EINVAL The manager has not been initialized.
    @retval    EBUSY  The file system has already registered.
    @remark    Set the name of the operation manager to
    fsm_ops->fsm_name, fsm_ops->fsm_fops, fsm_ops->fsm_super, fsm_ops->fsm_private.
    before calling this function.
 */
int
fs_vfs_register_filesystem(struct _fs_fs_manager *fsm_ops){
	int                     rc;
	struct _list          *itr;
	struct _fs_fs_manager *mgr;

	if ( !FS_FSMGR_IS_VALID(fsm_ops) )
		return EINVAL;

	if ( !list_not_linked(&fsm_ops->fsm_node) )
		return EBUSY;

	queue_for_each(itr, &fs_tbl.head){

		mgr = container_of(itr, struct _fs_fs_manager, fsm_node);
		if ( !strcmp(mgr->fsm_name, mgr->fsm_name) ) {

			rc = EBUSY;  /* The file system has already been registered. */
			goto error_out;
		}
	}

	/*
	 * Register the file system
	 */
	queue_add(&fs_tbl.head, &mgr->fsm_node);
	mgr->fsm_use_cnt = 0;  /* Reset use count */

	return 0;

error_out:
	return rc;
}

/** Unregister a file system
    @param[in] name   The name of  the storage manager to unregister
    @retval    0      success
    @retval    EBUSY  Some image files, managed by the storage operation to remove,
    are mounted.
*/
int
fs_vfs_unregister_filesystem(const char *name){
	int                     rc;
	struct _list          *itr;
	struct _fs_fs_manager *mgr;

	queue_for_each(itr, &fs_tbl.head){

		mgr = container_of(itr, struct _fs_fs_manager, fsm_node);
		if ( strcmp(name, mgr->fsm_name) )
			continue;

		if ( mgr->fsm_use_cnt > 0 ) {

			rc = EBUSY;  /* Some image files are mounted. */
			break;
		}

		/*
		 * Unregister the operation
		 */
		queue_del(&fs_tbl.head, &mgr->fsm_node);
		rc = 0;
		break;
	}

	return rc;
}

/** open a directory
    @param[in]  ch      The drive letter
    @param[in]  path    The path name of the directory to open
    @param[in]  ioctx   The current I/O context.
    @param[out] dirp    The pointer to the DIR structure (directory stream).
    @param[out] resp    The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -1      Error
    @retval     EINVAL  Invalid whence
    @retval     ENXIO   The new position exceeded the file size
 */
int
fs_vfs_opendir(sos_devltr ch, struct _fs_ioctx *ioctx,
    const char *path, struct _sword_dir *dirp, BYTE *resp){
	int                        rc;
	struct _sword_dir         dir;
	BYTE                      res;
	struct _fs_fs_manager *fs_mgr;

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		res = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	if ( rc != 0 ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	/*
	 * Check whether the file operation is defined.
	 */
	rc = ref_filesystem(ch, &fs_mgr); /* Refer file system */
	if ( !FS_FSMGR_FOP_IS_DEFINED(fs_mgr, fops_opendir) ) {

		res = SOS_ERROR_OFFLINE;  /* Not supported */
		goto error_out;
	}

	init_dir_stream(ch, ioctx, &dir);  /* Init the directory stream. */

	/* Call opendir */
	rc = fs_mgr->fsm_fops->fops_opendir(&dir, &res);
	if ( rc != 0 )
		goto error_out;

	memcpy(dirp, &dir, sizeof(struct _sword_dir));

	if ( resp != NULL )
		*resp = 0;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;

	return -1;
}

/** close a directory
    @param[in]  ch     The drive letter
    @param[in]  ioctx  The current I/O context.
    @param[in]  dirp   The pointer to the DIR structure (directory stream).
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
 */
int
fs_vfs_closedir(sos_devltr ch, struct _fs_ioctx *ioctx,
    struct _sword_dir *dirp, BYTE *resp){
	int                        rc;
	BYTE                      res;
	struct _fs_fs_manager *fs_mgr;

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		res = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	if ( rc != 0 ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	/*
	 * Check whether the file operation is defined.
	 */
	rc = ref_filesystem(ch, &fs_mgr); /* Refer file system */
	if ( !FS_FSMGR_FOP_IS_DEFINED(fs_mgr, fops_closedir) ) {

		res = SOS_ERROR_OFFLINE;  /* Not supported */
		goto error_out;
	}

	/* Call closedir */
	rc = fs_mgr->fsm_fops->fops_closedir(dirp, &res);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = 0;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;

	return -1;
}