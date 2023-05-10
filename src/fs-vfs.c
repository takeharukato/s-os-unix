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
static struct _fs_file_descriptor fd_tbl[FS_SYS_FDTBL_NR];

/** Initialize the file descriptor
    @param[in]  ioctx The current I/O context.
    @param[out] fdp   The address to the file descriptor (file handler) to initialize
 */
static void
init_fd(struct _fs_ioctx *ioctx, struct _fs_file_descriptor *fdp){
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	memset(fdp, 0x0, sizeof(struct _fs_file_descriptor));   /* just in case */

	fdp->fd_vnode = NULL;  /* Init v-node */
	storage_init_position(&fdp->fd_pos);  /* Initialize position */

	pos = &fdp->fd_pos;

	fdp->fd_use_cnt = 0;  /* Use count */

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
init_dir_stream(struct _fs_ioctx *ioctx, struct _fs_dir_stream *dir){

	init_fd(ioctx, &dir->dir_fd);
	dir->dir_private = NULL;
}

/** Initialize file descriptor table
 */
static void
fs_vfs_init_fdtbl(void){
	int i;

	for(i = 0; FS_SYS_FDTBL_NR > i; ++i)
		init_fd(NULL, &fd_tbl[i]);
}

/** Allocate global file descriptor and increment use count
    @param[in]  ioctx  The current I/O context.
    @param[out] fdnump The address of the pointer variable of the file descriptor number.
    @retval    0     success
    @retval    ENOSPC No free file descriptor found.
    @remark    The responsible for incrementing the v-node usage count of
    the v-node in the fd is fs_vfs_open function's concern.
 */
static int
alloc_fd(struct _fs_ioctx *ioctx, int *fdnump){
	int                     i, idx;
	struct _fs_file_descriptor *fd;

	for( idx = 0; FS_PROC_FDTBL_NR > idx; ++idx)
		if ( ioctx->ioc_fds[idx] == NULL )
			goto search_global_fd;
	goto not_found;

search_global_fd:
	for(i = 0, fd = &fd_tbl[0]; FS_SYS_FDTBL_NR > i; ++i, fd = &fd_tbl[i])
		if ( fd->fd_use_cnt == 0 )
			goto found;

not_found:
	return ENOSPC;

found:
	if ( fdnump != NULL ) {

		++fd->fd_use_cnt;
		ioctx->ioc_fds[idx] = fd;
		*fdnump = idx;
	}

	return 0;
}

/** Free global file descriptor (decrement use count)
    @param[in] ioctx  The current I/O context.
    @param[in] fdnum  The file descriptor number
    @retval    0      success
    @retval    EINVAL The file descriptor is not used.
    @retval    EBUSY  The file descriptor is still used.
    @remark    The responsible for decrementing the v-node usage count of
    the v-node in the fd is fs_vfs_close function's concern.
 */
static int
free_fd(struct _fs_ioctx *ioctx, int fdnum){
	struct _fs_file_descriptor *fd;

	sos_assert( ( fdnum >= 0 ) && ( FS_PROC_FDTBL_NR > fdnum ) );

	fd = ioctx->ioc_fds[fdnum];
	if ( fd->fd_use_cnt == 0 )
		return EINVAL;

	--fd->fd_use_cnt;

	if ( fd->fd_use_cnt > 0 )
		return EBUSY;

	init_fd(NULL, fd); /* clear fd */

	ioctx->ioc_fds[fdnum] = NULL;

	return 0;
}

/** Create a file
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] path      The filepath to create
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
 */
static int
create_file(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const char *path, const struct _sword_header_packet *pkt,
    BYTE *resp){
	int                        rc;
	vfs_mnt_flags       mnt_flags;
	struct _fs_vnode        *dirv;
	struct _fs_vnode           *v;
	char fname[SOS_UNIX_PATH_MAX];
	vfs_vnid                 vnid;
	BYTE                      res;

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		rc = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	rc = fs_vfs_get_mount_flags(ch, &mnt_flags);
	if ( rc != 0 ) {

		rc = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	rc = fs_vfs_path_to_dir_vnode(ch, ioctx, path, &dirv, fname, SOS_UNIX_PATH_MAX);
	if ( rc != 0 ) {

		res = rc;
		goto error_out;
	}

	if ( !FS_FSMGR_FOP_IS_DEFINED(v->vn_mnt->m_fs, fops_creat) ) {

		rc = SOS_ERROR_INVAL;
		goto put_dir_vnode_out;
	}

	rc = v->vn_mnt->m_fs->fsm_fops->fops_creat(ch, ioctx, dirv, fname, pkt,
	    &vnid, &res);
	if ( rc != 0 ) {

		rc = res;
		goto put_dir_vnode_out;
	}
	rc = 0;

put_dir_vnode_out:
	vfs_put_vnode(dirv);

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Open a file
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] path      The filepath to create
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] fdnump   The address to store a file descriptor number.
    @retval     0        Success
    @retval    -1        Error
 */
static int
fd_open_file(sos_devltr ch, struct _fs_ioctx *ioctx,
    const char *path, fs_open_flags flags, const struct _sword_header_packet *pkt,
    int *fdnump){
	int                            rc;
	int                         fdnum;
	struct _fs_vnode               *v;
	BYTE                          res;

	/*
	 * Open file
	 */
	rc = fs_vfs_path_to_vnode(ch, ioctx, path, &v);
	if ( rc != 0 ) {

		res = rc;
		goto error_out;
	}

	rc = alloc_fd(ioctx, &fdnum);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOSPC;
		goto put_vnode_out;
	}

	++v->vn_use_cnt;
	ioctx->ioc_fds[fdnum]->fd_vnode = v;
	ioctx->ioc_fds[fdnum]->fd_flags = flags;

	if ( !FS_FSMGR_FOP_IS_DEFINED(v->vn_mnt->m_fs, fops_open) ) {

		rc = SOS_ERROR_INVAL;
		goto free_fd_out;
	}

	rc = v->vn_mnt->m_fs->fsm_fops->fops_open(ch, ioctx, v, pkt, flags, &res);
	if ( rc == 0 ) {

		*fdnump = fdnum;
		goto put_vnode_out;
	}

	rc =  res;

free_fd_out:
	--v->vn_use_cnt;
	free_fd(ioctx, fdnum);

put_vnode_out:
	vfs_put_vnode(v);

error_out:
	return rc;
}

/** Initialize file manager
    @param[out] fsm file manager to init
 */
void
fs_vfs_init_file_manager(struct _fs_fs_manager *fsm){

	list_init(&fsm->fsm_node);
	fsm->fsm_use_cnt = 0;
	fsm->fsm_name = NULL;
	fsm->fsm_fops = NULL;
	fsm->fsm_private = NULL;
}

/** Initialize the I/O context
    @param[in]  ioctx  The current I/O context.
 */
void
fs_vfs_init_ioctx(struct _fs_ioctx *ioctx){
	int  i;

	for( i = 0; STORAGE_NR > i; ++i) {

		/* Initialize root fib and current working directory */
		ioctx->ioc_root[i] = NULL;
		ioctx->ioc_cwd[i]  = NULL;
	}

	for( i = 0; FS_PROC_FDTBL_NR > i; ++i)
		ioctx->ioc_fds[i] = NULL;

	ioctx->ioc_dirps = SOS_DIRPS_DEFAULT;
	ioctx->ioc_fatpos = SOS_FATPOS_DEFAULT;
}

/** Look up a file system
    @param[in]  name  The file system name
    @param[out] fsmp  The address of the pointer variable to point the file system manager
    @retval    0     success
    @retval    EINVAL The manager has not been initialized.
    @retval    EBUSY  The file system has already registered.
    @remark    Set the name of the operation manager to
    fsm_ops->fsm_name, fsm_ops->fsm_fops, fsm_ops->fsm_super, fsm_ops->fsm_private.
    before calling this function.
 */
int
fs_vfs_lookup_filesystem(const char *name, struct _fs_fs_manager **fsmp){
	struct _list          *itr;
	struct _fs_fs_manager *mgr;

	queue_for_each(itr, &fs_tbl.head){

		mgr = container_of(itr, struct _fs_fs_manager, fsm_node);
		if ( !strcmp(name, mgr->fsm_name) )
			goto found;
	}

	return ENOENT;  /* The file system is not found. */

found:
	if ( fsmp != NULL )
		*fsmp = mgr;

	return 0;
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
fs_vfs_register_filesystem(struct _fs_fs_manager *fsm){
	int                     rc;
	struct _list          *itr;
	struct _fs_fs_manager *mgr;

	if ( !FS_FSMGR_IS_VALID(fsm) )
		return EINVAL;

	if ( !list_not_linked(&fsm->fsm_node) )
		return EBUSY;

	queue_for_each(itr, &fs_tbl.head){

		mgr = container_of(itr, struct _fs_fs_manager, fsm_node);
		if ( !strcmp(fsm->fsm_name, mgr->fsm_name) ) {

			rc = EBUSY;  /* The file system has already been registered. */
			goto error_out;
		}
	}

	/*
	 * Register the file system
	 */
	queue_add(&fs_tbl.head, &fsm->fsm_node);
	fsm->fsm_use_cnt = 0;  /* Reset use count */

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

/** Create a file
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] path      The filepath to create
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] fdnump   The address to store a file descriptor number.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
 */
int
fs_vfs_creat(sos_devltr ch, struct _fs_ioctx *ioctx,
    const char *path, const struct _sword_header_packet *pkt,
    int *fdnump, BYTE *resp){
	int                        rc;
	vfs_mnt_flags       mnt_flags;
	BYTE                      res;
	int                     fdnum;
	struct _fs_vnode           *v;
	char fname[SOS_UNIX_PATH_MAX];

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		rc = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	rc = fs_vfs_get_mount_flags(ch, &mnt_flags);
	if ( rc != 0 ) {

		rc = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	if ( mnt_flags & FS_VFS_MNT_OPT_RDONLY ) {

		rc = SOS_ERROR_RDONLY;  /* Read only mount */
		goto error_out;
	}

	/* Create a file */
	rc = create_file(ch, ioctx, path, pkt, &res);
	if ( rc != 0 ) {

		rc = res;
		goto error_out;
	}

	/* open the file */
	rc = fd_open_file(ch, ioctx, path,
	    FS_VFS_FD_FLAG_O_WRONLY|FS_VFS_FD_FLAG_O_CREAT, pkt, &fdnum);
	if ( rc != 0 )
		goto error_out;

	if ( fdnump != NULL )
		*fdnump = fdnum;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Open a file
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] path      The filepath to create
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] fdnump   The address to store a file descriptor number.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
 */
int
fs_vfs_open(sos_devltr ch, struct _fs_ioctx *ioctx,
    const char *path, fs_open_flags flags, const struct _sword_header_packet *pkt,
    int *fdnump, BYTE *resp){
	int                            rc;
	int                         fdnum;
	vfs_mnt_flags           mnt_flags;
	struct _fs_vnode               *v;
	char     fname[SOS_UNIX_PATH_MAX];
	BYTE                          res;

	sos_assert( fdnump != NULL );

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		rc = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	rc = fs_vfs_get_mount_flags(ch, &mnt_flags);
	if ( rc != 0 ) {

		rc = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE )
	    && ( mnt_flags & FS_VFS_MNT_OPT_RDONLY ) ) {

		rc = SOS_ERROR_RDONLY;  /* Read only mount */
		goto error_out;
	}

	/*
	 * Create a file
	 */
	if ( flags & FS_VFS_FD_FLAG_O_CREAT ) {

		if ( mnt_flags & FS_VFS_MNT_OPT_RDONLY ) {

			rc = SOS_ERROR_RDONLY;  /* Read only mount */
			goto error_out;
		}

		/* Create a file */
		rc = create_file(ch, ioctx, path, pkt, &res);
		if ( rc != 0 ) {

			rc = res;
			goto error_out;
		}
	}

	/*
	 * Open file
	 */
	rc = fd_open_file(ch, ioctx, path, flags, pkt, &fdnum);
	if ( rc != 0 )
		goto error_out;

	if ( fdnump != NULL )
		*fdnump = fdnum;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Close a file
    @param[in]  ioctx The current I/O context.
    @param[in]  fdnum A file descriptor number in The I/O context.
    @param[out] resp  The address to store the return code.
 */
int
fs_vfs_close(struct _fs_ioctx *ioctx, int fdnum, BYTE *resp){
	int                          rc;
	BYTE                        res;
	struct _fs_file_descriptor *fdp;

	if ( ( 0 > fdnum ) || ( fdnum >= FS_PROC_FDTBL_NR ) ) {

		rc = SOS_ERROR_SYNTAX;
		goto error_out;
	}

	fdp = ioctx->ioc_fds[fdnum];
	if ( fdp == NULL )  {

		rc = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	sos_assert(fdp->fd_vnode->vn_use_cnt > 0 );
	--fdp->fd_vnode->vn_use_cnt;

	free_fd(ioctx, fdnum);

	rc = 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Initialize virtual file system
 */
void
fs_vfs_init_vfs(void){

	fs_vfs_init_vnode_tbl();
	fs_vfs_init_mount_tbl();
	fs_vfs_init_fdtbl();
}
