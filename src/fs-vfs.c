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
	return SOS_ERROR_NOSPC;

found:
	if ( fdnump != NULL ) {

		++fd->fd_use_cnt;
		ioctx->ioc_fds[idx] = fd;
		fd->fd_ioctx = ioctx;
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

	fd->fd_sysflags &= ~FS_VFS_FD_FLAG_SYS_OPENED;
	init_fd(NULL, fd); /* clear fd */

	ioctx->ioc_fds[fdnum] = NULL;

	return 0;
}

/** Initialize the directory stream
    @param[in]  ch    The drive letter
    @param[in]  ioctx The current I/O context.
    @param[out] dir   The address to the directory stream to initialize
    @retval    0     success
    @retval    ENOSPC No free file descriptor found.
 */
static int
init_dir_stream(struct _fs_ioctx *ioctx, struct _fs_dir_stream *dir){
	int rc;
	int fd;

	dir->dir_fd = NULL;
	dir->dir_private = NULL;

	rc = alloc_fd(ioctx, &fd);
	if ( rc != 0 )
		goto error_out;

	dir->dir_fd = ioctx->ioc_fds[fd];

error_out:
	return rc;
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
	struct _fs_vnode        *dirv;
	char fname[SOS_UNIX_PATH_MAX];
	vfs_vnid                 vnid;
	BYTE                      res;

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		rc = SOS_ERROR_OFFLINE;
		goto error_out;
	}

	rc = fs_vfs_path_to_dir_vnode(ch, ioctx, path, &dirv, fname, SOS_UNIX_PATH_MAX);
	if ( rc != 0 ) {

		res = rc;
		goto error_out;
	}

	if ( !FS_FSMGR_FOP_IS_DEFINED(dirv->vn_mnt->m_fs, fops_creat) ) {

		rc = SOS_ERROR_INVAL;
		goto put_dir_vnode_out;
	}

	rc = dirv->vn_mnt->m_fs->fsm_fops->fops_creat(ch, ioctx, dirv, fname, pkt,
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

	/*
	 * Check write protect bit
	 */
	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE)
	    && ( SOS_FATTR_MASK_SOS_ATTR(v->vn_fib.fib_attr) & SOS_FATTR_RDONLY) ) {

		res = SOS_ERROR_RDONLY;
		goto error_out;
	}

	/*
	 * The file system does not support the write operation.
	 */
	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE)
	    && ( !FS_FSMGR_FOP_IS_DEFINED(v->vn_mnt->m_fs, fops_write) ) ){

		res = SOS_ERROR_RDONLY;
		goto error_out;
	}

	rc = alloc_fd(ioctx, &fdnum);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOSPC;
		goto put_vnode_out;
	}

	vfs_inc_vnode_cnt(v);
	ioctx->ioc_fds[fdnum]->fd_vnode = v;
	ioctx->ioc_fds[fdnum]->fd_flags = flags;

	if ( !FS_FSMGR_FOP_IS_DEFINED(v->vn_mnt->m_fs, fops_open) )
		goto success;

	rc = v->vn_mnt->m_fs->fsm_fops->fops_open(ioctx->ioc_fds[fdnum],
	    pkt, flags, &res);
	if ( rc != 0 )
		goto free_fd_out;
success:
	rc =  res;
	ioctx->ioc_fds[fdnum]->fd_sysflags |= FS_VFS_FD_FLAG_SYS_OPENED;
	*fdnump = fdnum;

	goto put_vnode_out;


free_fd_out:
	vfs_dec_vnode_cnt(v);
	free_fd(ioctx, fdnum);

put_vnode_out:
	vfs_put_vnode(v);

error_out:
	return rc;
}

/** Check a device
    @param[in] ch        The drive letter
    @param[in] flags     Open flags
    @retval     0        Success
    @retval    SOS_ERROR_OFFLINE Device offline
    @retval    SOS_ERROR_RDONLY  Readonly mount
 */
static int
check_device(sos_devltr ch, fs_open_flags flags){
	int                        rc;
	vfs_mnt_flags       mnt_flags;

	rc = storage_check_status(ch);
	if ( rc != 0 )
		return SOS_ERROR_OFFLINE;

	rc = fs_vfs_get_mount_flags(ch, &mnt_flags);
	if ( rc != 0 )
		return SOS_ERROR_OFFLINE;

	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE )
	    && ( mnt_flags & FS_VFS_MNT_OPT_RDONLY ) )
		return  SOS_ERROR_RDONLY;  /* Read only mount */

	return 0;
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
	BYTE                      res;
	int                     fdnum;
	struct _fs_vnode           *v;
	char fname[SOS_UNIX_PATH_MAX];

	rc = check_device(ch, FS_VFS_FD_FLAG_O_CREAT);
	if ( rc != 0 )
		goto error_out;

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

/** Unlink a file
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] path      The filepath to create
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
 */
int
fs_vfs_unlink(sos_devltr ch, struct _fs_ioctx *ioctx, const char *path, BYTE *resp){
	int                        rc;
	BYTE                      res;
	struct _fs_vnode        *dirv;
	char fname[SOS_UNIX_PATH_MAX];

	rc = check_device(ch, FS_VFS_FD_FLAG_O_CREAT);
	if ( rc != 0 )
		goto error_out;

	rc = fs_vfs_path_to_dir_vnode(ch, ioctx, path, &dirv, fname, SOS_UNIX_PATH_MAX);
	if ( rc != 0 ) {

		res = rc;
		goto error_out;
	}

	if ( !FS_FSMGR_FOP_IS_DEFINED(dirv->vn_mnt->m_fs, fops_unlink) ) {

		res = SOS_ERROR_INVAL;
		goto put_dir_vnode_out;
	}

	rc = dirv->vn_mnt->m_fs->fsm_fops->fops_unlink(ch, ioctx, dirv, fname, &res);
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
    @param[in] path      The filepath to open
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
	struct _fs_vnode               *v;
	char     fname[SOS_UNIX_PATH_MAX];
	BYTE                          res;

	sos_assert( fdnump != NULL );

	rc = check_device(ch, flags);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Create a file
	 */
	if ( flags & FS_VFS_FD_FLAG_O_CREAT ) {

		/* Create a file */
		rc = create_file(ch, ioctx, path, pkt, &res);
		if ( ( rc != 0 ) && ( ( rc != SOS_ERROR_EXIST ) ||
			( flags & FS_VFS_FD_FLAG_O_EXCL ) ) ) {

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

	if ( !FS_FSMGR_FOP_IS_DEFINED(fdp->fd_vnode->vn_mnt->m_fs, fops_close) )
		goto close_fd;

	rc = fdp->fd_vnode->vn_mnt->m_fs->fsm_fops->fops_close(fdp, &res);
	if ( rc != 0 ) {

		rc = res;
		goto error_out;
	}

close_fd:
	rc = 0;
	sos_assert(fdp->fd_vnode->vn_use_cnt > 0 );
	vfs_dec_vnode_cnt(fdp->fd_vnode);

	free_fd(ioctx, fdnum);

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Read from a file
    @param[in]  ioctx  The current I/O context.
    @param[in]  fd     A file descriptor number in The I/O context.
    @param[out] buf    The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] rwcntp  The adress to store read bytes.
    @param[out] resp    The address to store the return code for S-OS.
    @retval     0                Success
    @retval    -1                Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
    * SOS_ERROR_NOTOPEN The file is not opend
 */
int
fs_vfs_read(struct _fs_ioctx *ioctx, int fd, void *buf, size_t count,
    size_t *rwcntp, BYTE *resp){
	int                          rc;
	size_t                    rdsiz;
	BYTE                        res;
	struct _storage_disk_pos   *pos;
	struct _fs_file_descriptor *fdp;

	if ( ( 0 > fd ) || ( fd >= FS_PROC_FDTBL_NR ) ) {

		res = SOS_ERROR_INVAL;
		goto error_out;
	}

	fdp = ioctx->ioc_fds[fd];
	pos = &fdp->fd_pos;

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	rdsiz = 0;  /* Init read size */

	if ( !FS_FSMGR_FOP_IS_DEFINED(fdp->fd_vnode->vn_mnt->m_fs, fops_read) ){

		res = 0; /* End of file */
		goto update_pos;
	}

	rc = fdp->fd_vnode->vn_mnt->m_fs->fsm_fops->fops_read(fdp, buf, count,
	    &rdsiz, &res);
	if ( rc != 0 ) {

		pos->dp_pos += rdsiz;  /* update position */
		goto error_out;
	}

update_pos:
	pos->dp_pos += rdsiz;  /* update position */

	res = 0;

error_out:
	if ( rwcntp != NULL )
		*rwcntp = rdsiz;

	if ( resp != NULL )
		*resp = res;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(res);  /* return code */

	return (res == 0) ? (0) : (-1);
}

/** Write to a file
    @param[in]  ioctx  The current I/O context.
    @param[in]  fd     A file descriptor number in The I/O context.
    @param[in]  buf    The buffer to write from
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] rwcntp  The adress to store written bytes.
    @param[out] resp    The address to store the return code for S-OS.
    @retval     0                Success
    @retval    -1                Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
    * SOS_ERROR_NOTOPEN The file is not opend
 */
int
fs_vfs_write(struct _fs_ioctx *ioctx, int fd, const void *buf, size_t count,
    size_t *rwcntp, BYTE *resp){
	int                        rc;
	size_t                  wrsiz;
	BYTE                      res;
	struct _storage_disk_pos *pos;
	struct _fs_file_descriptor *fdp;

	if ( ( 0 > fd ) || ( fd >= FS_PROC_FDTBL_NR ) ) {

		res = SOS_ERROR_INVAL;
		goto error_out;
	}

	fdp = ioctx->ioc_fds[fd];
	pos = &fdp->fd_pos;

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	if ( !( fdp->fd_flags & FS_VFS_FD_FLAG_MAY_WRITE ) ) {

		res = SOS_ERROR_NOTOPEN;  /* The file is not opend to write. */
		goto error_out;
	}

	wrsiz = 0;  /* Init written size */

	/* The fs_vfs_open function has maked sure the write operation handler exists. */
	sos_assert( FS_FSMGR_FOP_IS_DEFINED(fdp->fd_vnode->vn_mnt->m_fs, fops_write) );

	rc = fdp->fd_vnode->vn_mnt->m_fs->fsm_fops->fops_write(fdp, buf, count,
	    &wrsiz, &res);
	if ( rc != 0 ) {

		pos->dp_pos += wrsiz;  /* update position */
		goto error_out;
	}

update_pos:
	pos->dp_pos += wrsiz;  /* update position */

	res = 0;

error_out:
	if ( rwcntp != NULL )
		*rwcntp = wrsiz;

	if ( resp != NULL )
		*resp = res;

	return (res == 0) ? (0) : (-1);
}

/** Truncate a file to a specified length
    @param[in]  ioctx  The current I/O context.
    @param[in]  fd     A file descriptor number in The I/O context.
    @param[in]  offset The file length of the file to be truncated.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fs_vfs_truncate(struct _fs_ioctx *ioctx, int fd, fs_off_t offset, BYTE *resp){
	int                          rc;
	BYTE                        res;
	struct _fs_file_descriptor *fdp;

	if ( ( 0 > fd ) || ( fd >= FS_PROC_FDTBL_NR ) ) {

		res = SOS_ERROR_INVAL;
		goto error_out;
	}

	fdp = ioctx->ioc_fds[fd];

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	if ( !FS_FSMGR_FOP_IS_DEFINED(fdp->fd_vnode->vn_mnt->m_fs, fops_truncate) ){

		res = SOS_ERROR_INVAL;
		goto error_out;
	}


	rc = fdp->fd_vnode->vn_mnt->m_fs->fsm_fops->fops_truncate(fdp, offset, &res);
	if ( ( rc != 0 ) || ( res != 0 ) )
		goto error_out;

error_out:
	if ( resp != NULL )
		*resp = res;

	return (res == 0) ? (0) : (-1);
}
/** Stat a file
    @param[in]  ioctx  The current I/O context.
    @param[in]  fd     A file descriptor number in The I/O context.
    @param[out] fib    The address to store the file information block.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fs_vfs_stat(struct _fs_ioctx *ioctx, int fd,
    struct _storage_fib *fib, BYTE *resp){
	int                          rc;
	BYTE                        res;
	struct _fs_file_descriptor *fdp;

	if ( ( 0 > fd ) || ( fd >= FS_PROC_FDTBL_NR ) ) {

		res = SOS_ERROR_INVAL;
		goto error_out;
	}

	fdp = ioctx->ioc_fds[fd];

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	memcpy(fib, &fdp->fd_vnode->vn_fib, sizeof(struct _storage_fib));

error_out:
	if ( resp != NULL )
		*resp = res;

	return (res == 0) ? (0) : (-1);
}

/** Seek a file
    @param[in]  ioctx  The current I/O context.
    @param[in]  fd     A file descriptor number in The I/O context.
    @param[in]  offset  The offset to reposition according to WHENCE
    excluding the size of the S-OS header.
    @param[in]  whence  The directive to reposition:
     FS_VFS_SEEK_SET The file offset is set to offset bytes.
     FS_VFS_SEEK_CUR The file offset is set to its current location plus offset bytes.
     FS_VFS_SEEK_END The file offset is set to the size of the file plus offset bytes.
    @param[out] new_posp The address to store the new position.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0                Success
    @retval    -EINVAL           Invalid whence
 */
int
fs_vfs_seek(struct _fs_ioctx *ioctx, int fd, fs_off_t offset,
    int whence, fs_off_t *new_posp, BYTE *resp){
	int                          rc;
	BYTE                        res;
	struct _fs_file_descriptor *fdp;
	struct _storage_disk_pos   *pos;
	struct _storage_fib        *fib;
	fs_off_t                    new;
	fs_off_t                    cur;
	fs_off_t                    off;

	if ( ( 0 > fd ) || ( fd >= FS_PROC_FDTBL_NR ) ) {

		res = SOS_ERROR_INVAL;
		goto error_out;
	}

	fdp = ioctx->ioc_fds[fd];
	pos = &fdp->fd_pos;            /* Position information */
	fib = &fdp->fd_vnode->vn_fib;  /* File information block */

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	/* Adjust offset according to the max file size */
	if ( offset > 0 )
		off = SOS_MIN(offset, SOS_MAX_FILE_SIZE);
	else if ( 0 > offset )
		off = SOS_MAX(offset, (fs_off_t)-1 * SOS_MAX_FILE_SIZE);

	/*
	 * Calculate the start position
	 */
	switch( whence ) {

	case FS_VFS_SEEK_SET:
		cur = 0;
		break;

	case FS_VFS_SEEK_CUR:
		cur = SOS_MIN(pos->dp_pos, SOS_MAX_FILE_SIZE);
		break;

	case FS_VFS_SEEK_END:

		cur = SOS_MIN(fib->fib_size, SOS_MAX_FILE_SIZE);
		break;

	default:

		if ( resp != NULL )
			*resp = SOS_ERROR_SYNTAX;  /* return code */
		return -1;
	}

	if ( 0 > ( cur + off ) )
		new = 0;
	else if ( off > ( SOS_MAX_FILE_SIZE - cur ) )
		new = SOS_MAX_FILE_SIZE;
	else
		new = cur + off;

	/*
	 * File system specific seek
	 */
	if ( !FS_FSMGR_FOP_IS_DEFINED(fdp->fd_vnode->vn_mnt->m_fs, fops_seek) )
		goto success;

	rc = fdp->fd_vnode->vn_mnt->m_fs->fsm_fops->fops_seek(fdp, offset,
	    whence, &new, &res);
	if ( ( rc != 0 ) || ( res != 0 ) )
		goto error_out;

success:
	pos->dp_pos = new;  /* Update position */

error_out:
	if ( resp != NULL )
		*resp = res;

	if ( new_posp != NULL )
		*new_posp = new;

	return (res == 0) ? (0) : (-1);
}

/** Rename the file
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] oldpath   The filepath to rename
    @param[in] newpath   The new filepath
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
*/
int
fs_vfs_rename(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const char *oldpath, const char *newpath, BYTE *resp){
	int                             rc;
	BYTE                           res;
	struct _fs_vnode                *v;
	struct _fs_vnode           *src_vn;
	struct _fs_vnode          *dest_vn;
	vfs_vnid              old_dir_vnid;
	char   src_name[SOS_UNIX_PATH_MAX];
	char  dest_name[SOS_UNIX_PATH_MAX];

	rc = check_device(ch, FS_VFS_FD_FLAG_O_CREAT);
	if ( rc != 0 )
		goto error_out;

	/* Check old path is writable */
	rc = fs_vfs_path_to_vnode(ch, ioctx, oldpath, &v);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Check write protect bit
	 */
	if ( SOS_FATTR_MASK_SOS_ATTR(v->vn_fib.fib_attr) & SOS_FATTR_RDONLY) {

		res = SOS_ERROR_RDONLY;
		goto put_file_vnode_out;
	}

	/*
	 * Get the v-node of old path
	 */
	rc = fs_vfs_path_to_dir_vnode(ch, ioctx, oldpath, &src_vn,
	    src_name, SOS_UNIX_PATH_MAX);
	if ( rc != 0 ) {

		res = rc;
		goto put_file_vnode_out;
	}

	old_dir_vnid = src_vn->vn_id;
	vfs_put_vnode(src_vn);  /* Put v-node at once to avoid self dead lock */

	/*
	 * Get the v-node of new path
	 */
	rc = fs_vfs_path_to_dir_vnode(ch, ioctx, newpath, &dest_vn,
	    dest_name, SOS_UNIX_PATH_MAX);
	if ( rc != 0 ) {

		res = rc;
		goto put_file_vnode_out;
	}

	if ( old_dir_vnid != dest_vn->vn_id ) {  /* Different directory case */

		/*
		 * Get the v-node of old path
		 */
		rc = fs_vfs_path_to_dir_vnode(ch, ioctx, oldpath, &src_vn,
		    src_name, SOS_UNIX_PATH_MAX);
		if ( rc != 0 ) {

			res = rc;
			goto put_dest_vnode_out;
		}
	}

	if ( !FS_FSMGR_FOP_IS_DEFINED(src_vn->vn_mnt->m_fs, fops_rename) ){

		res = SOS_ERROR_INVAL;
		goto put_dest_vnode_out;
	}


	/* Rename */
	rc = src_vn->vn_mnt->m_fs->fsm_fops->fops_rename(ch, ioctx, src_vn, src_name,
	    dest_vn, dest_name, &res);
	if ( ( rc != 0 ) || ( res != 0 ) )
		goto put_src_vnode_out;

put_src_vnode_out:
		vfs_put_vnode(src_vn);

put_dest_vnode_out:
		if ( old_dir_vnid != dest_vn->vn_id )
			vfs_put_vnode(dest_vn);

put_file_vnode_out:
		vfs_put_vnode(v);

error_out:
	if ( resp != NULL )
		*resp = res;

	return (res == 0) ? (0) : (-1);
}

/** Set the file attribute
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] path      The filepath to change attribute
    @param[in] attr      The new attribute.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
*/
int
fs_vfs_set_attr(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const char *path, const fs_attr attr, BYTE *resp){
	int              rc;
	BYTE            res;
	struct _fs_vnode *v;

	/*
	 * Get file v-node
	 */
	rc = fs_vfs_path_to_vnode(ch, ioctx, path, &v);
	if ( rc != 0 ) {

		res = rc;
		goto error_out;
	}

	if ( !FS_FSMGR_FOP_IS_DEFINED(v->vn_mnt->m_fs, fops_set_attr) ){

		res = SOS_ERROR_INVAL;
		goto error_out;
	}


	/* Set attribute */
	rc = v->vn_mnt->m_fs->fsm_fops->fops_set_attr(ch, ioctx, v, attr, &res);
	if ( ( rc != 0 ) || ( res != 0 ) )
		goto error_out;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Get the file attribute
    @param[in] ch        The drive letter.
    @param[in] ioctx     The current I/O context.
    @param[in] path      The filepath to change attribute.
    @param[out] attrp    The address to store the attribute.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
*/
int
fs_vfs_get_attr(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const char *path, fs_attr *attrp, BYTE *resp){
	int              rc;
	BYTE            res;
	fs_attr        attr;
	struct _fs_vnode *v;

	/*
	 * Get file v-node
	 */
	rc = fs_vfs_path_to_vnode(ch, ioctx, path, &v);
	if ( rc != 0 ) {

		res = rc;
		goto error_out;
	}

	attr = v->vn_fib.fib_attr;  /* Get attribute in v-node */

	if ( !FS_FSMGR_FOP_IS_DEFINED(v->vn_mnt->m_fs, fops_get_attr) )
		goto skip_fop;

	/* Get attribute */
	rc = v->vn_mnt->m_fs->fsm_fops->fops_get_attr(ch, ioctx, v, &attr, &res);
	if ( ( rc != 0 ) || ( res != 0 ) )
		goto error_out;
skip_fop:
	if ( attrp != NULL )
		*attrp = attr;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Open a directory
    @param[in] ch       The drive letter.
    @param[in] ioctx    The current I/O context.
    @param[in] path     Directory path.
    @param[out] dir     The pointer to the directory stream.
    @param[out] resp    The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -1      Error
    @retval     EINVAL  Invalid whence
    @retval     ENXIO   The new position exceeded the file size
 */
int
fs_vfs_opendir(sos_devltr ch, struct _fs_ioctx *ioctx, const char *path,
    struct _fs_dir_stream *dirp, BYTE *resp){
	int                          rc;
	BYTE                        res;
	struct _fs_dir_stream       dir;
	struct _fs_vnode          *dirv;
	char    name[SOS_UNIX_PATH_MAX];

	/*
	 * Get directory v-node
	 */
	rc = fs_vfs_path_to_dir_vnode(ch, ioctx, path, &dirv,
	    name, SOS_UNIX_PATH_MAX);
	if ( rc != 0 )
		goto error_out;

	rc = init_dir_stream(ioctx, &dir);
	if ( rc != 0 )
		goto put_vnode_out;

	vfs_inc_vnode_cnt(dirv);  /* increment use count */

	/*
	 * Set the file descriptor up
	 */
	dir.dir_fd->fd_vnode = dirv;
	dir.dir_fd->fd_flags = FS_VFS_FD_FLAG_O_RDONLY;
	dir.dir_fd->fd_sysflags |= FS_VFS_FD_FLAG_SYS_OPENED;

	if ( !FS_FSMGR_FOP_IS_DEFINED(dirv->vn_mnt->m_fs, fops_opendir) )
		goto put_vnode_out;

	/* File system specific opendir */
	rc = dirv->vn_mnt->m_fs->fsm_fops->fops_opendir(&dir, &res);
	if ( ( rc != 0 ) || ( res != 0 ) )
		goto put_vnode_out;

	if ( dirp != NULL )
		memcpy(dirp, &dir, sizeof(struct _fs_dir_stream));

put_vnode_out:
	vfs_put_vnode(dirv);

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** close a directory
    @param[out] dir     The pointer to the directory stream.
    @param[out] resp    The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -1      Error
    @retval     EINVAL  Invalid whence
    @retval     ENXIO   The new position exceeded the file size
 */
int
fs_vfs_closedir(struct _fs_dir_stream *dir, BYTE *resp){
	int                 rc;
	int                 fd;
	BYTE               res;
	struct _fs_vnode *dirv;

	dirv = dir->dir_fd->fd_vnode;
	if ( !FS_FSMGR_FOP_IS_DEFINED(dirv->vn_mnt->m_fs, fops_closedir) )
		goto success;

	/* File system specific closedir */
	rc = dirv->vn_mnt->m_fs->fsm_fops->fops_closedir(dir, &res);
	if ( ( rc != 0 ) || ( res != 0 ) )
		goto error_out;
success:
	rc = 0;
	sos_assert(dirv->vn_use_cnt > 0 );
	vfs_dec_vnode_cnt(dirv);
	fd = dir->dir_fd - dir->dir_fd->fd_ioctx->ioc_fds[0];
	free_fd(dir->dir_fd->fd_ioctx, fd);

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
