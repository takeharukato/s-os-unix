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
	.fops_lookup = fops_lookup_sword,
	.fops_creat = fops_creat_sword,
	.fops_open = fops_open_sword,
	.fops_close = fops_close_sword,
	.fops_unlink = fops_unlink_sword,
	.fops_read = fops_read_sword,
	.fops_write = fops_write_sword,
	.fops_truncate = fops_truncate_sword,
	.fops_stat = fops_stat_sword,
	.fops_rename = fops_rename_sword,
	.fops_set_attr = fops_set_attr_sword,
	.fops_get_attr = fops_get_attr_sword,
	.fops_opendir = fops_opendir_sword,
	.fops_readdir = fops_readdir_sword,
	.fops_seekdir = fops_seekdir_sword,
	.fops_telldir = fops_telldir_sword,
	.fops_closedir = fops_closedir_sword
};
static struct _fs_fs_manager sword_fsm;

/** Truncate a file to a specified length
    @param[in]  ioctx  The current I/O context
    @param[in]  vn     The vnode of the file
    @param[in]  fib    The file information block of the file.
    @param[in]  newpos The file position of the file to be truncated.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
static int
change_filesize_sword(const struct _fs_ioctx *ioctx, const struct _fs_vnode *vn,
    struct _storage_fib *fib, fs_off_t newpos){
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

	rc = fs_swd_write_dent(fib->fib_devltr, ioctx, vn, fib); /* write back */
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
	struct _storage_fib      *fib;

	vnid = FS_SWD_ROOT_VNID;  /* Root v-node */
	rc = vfs_vnode_get_free_vnode(&vn);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Fill File Information Block
	 */
	fib = &vn->vn_fib;

	fib->fib_devltr = ch;
	fib->fib_attr = SOS_FATTR_DIR;
	fib->fib_vnid = vnid;
	fib->fib_size = SOS_DENTRY_LEN;
	fib->fib_cls = SOS_REC2CLS(ioctx->ioc_dirps);

	memset(&fib->fib_sword_name[0], SCR_SOS_SPC, SOS_FNAME_LEN);

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

/** Get v-node in the directory entry (the v-node table)
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
		rc = fs_swd_search_fib_by_vnid(ch, ioctx, vnid, &fib);
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

/** look up v-node in the directory entry by name
    @param[in] ch         The drive letter
    @param[in] ioctx      The current I/O context
    @param[in] dir_vnode  The directory v-node
    @param[in] name       The file name
    @param[out] vnidp     The address to store v-node ID.
    @retval    0          Success
 */
int
fops_lookup_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const struct _fs_vnode *dir_vnode, const char *name, vfs_vnid *vnidp){
	int                       rc;
	BYTE swd_name[SOS_FNAME_LEN];
	vfs_vnid                vnid;

	/* Convert file name */
	rc = fs_unix2sword(name, &swd_name[0], SOS_FNAME_LEN);
	if ( rc != 0 )
		goto error_out;

	/* Search directory entry */
	rc = fs_swd_search_dent_by_name(ch, ioctx, dir_vnode,
	    &swd_name[0], &vnid);
	if ( rc != 0 )
		goto error_out;

	if ( vnidp != NULL )
		*vnidp = vnid;  /* Return vnid */

	return 0;

error_out:
	return rc;
}

/** Create a file
    @param[in] ch       The drive letter
    @param[in] ioctx    The current I/O context
    @param[in] dir_vn   The directory v-node
    @param[in] name     The file name to create
    @param[in]  pkt     The S-OS packet.
    @param[out] resp    The address to store the return code for S-OS.
    @retval     0       Success
    @retval    -1       Error
    The responses from the function:
    * SOS_ERROR_IO      I/O Error
    * SOS_ERROR_EXIST   File Already Exists
    * SOS_ERROR_NOSPC   Device Full (No free directory entry)
 */
int
fops_creat_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    struct _fs_vnode *dir_vn, const char *name,
    const struct _sword_header_packet *pkt, vfs_vnid *new_vnidp, BYTE *resp){
	int                       rc;
	BYTE swd_name[SOS_FNAME_LEN];
	struct _storage_fib      fib;
	vfs_vnid                vnid;

	/* Convert file name */
	rc = fs_unix2sword(name, &swd_name[0], SOS_FNAME_LEN);
	if ( rc != 0 )
		goto error_out;

	/* Check whether the file name does not exist. */
	rc = fs_swd_search_dent_by_name(ch, ioctx, dir_vn,
	    &swd_name[0], &vnid);
	if ( rc == 0 ) {

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/* Search a free directory entry */
	rc = fs_swd_search_free_dent(ch, ioctx, dir_vn, &vnid);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Fill fib
	 */
	storage_init_fib(&fib);
	fib.fib_devltr = ch;
	fib.fib_attr = pkt->hdr_attr;
	fib.fib_vnid = vnid;
	memcpy(&fib.fib_sword_name[0], &swd_name[0], SOS_FNAME_LEN);

	/*
	 * Write the directory entry
	 */
	rc = fs_swd_write_dent(ch, ioctx, dir_vn, &fib);
	if ( rc != 0 )
		goto error_out;

	rc = 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Unlink a file
    @param[in] ch       The drive letter
    @param[in] ioctx    The current I/O context
    @param[in] dir_vn   The directory v-node
    @param[in] name     The file name to unlink
    @param[out] resp    The address to store the return code for S-OS.
    @retval     0       Success
    @retval    -1       Error
    The responses from the function:
    * SOS_ERROR_IO      I/O Error
    * SOS_ERROR_NOENT   File not found
 */
int
fops_unlink_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    struct _fs_vnode *dir_vn, const char *name, BYTE *resp){
	int                             rc;
	struct _storage_fib            fib;
	struct _storage_fib       *dir_fib;
	vfs_vnid                      vnid;
	BYTE        swdname[SOS_FNAME_LEN];

	/* Get the filename of path in SWORD representation. */
	rc = fs_unix2sword(name, &swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to unlink. */
	rc = fs_swd_search_dent_by_name(ch, ioctx, dir_vn, &swdname[0], &vnid);
	if ( rc != 0 )
		goto error_out;

	/* Get the file information block */
	rc = fs_swd_search_fib_by_vnid(ch, ioctx, vnid, &fib);
	if ( rc != 0 )
		goto error_out;

	if ( fib.fib_attr & SOS_FATTR_RDONLY ) {

		rc = SOS_ERROR_RDONLY;  /* Write protected file */
		goto error_out;
	}

	if ( !SOS_FATTR_IS_REGULAR_FILE(fib.fib_attr) ) {

		rc = SOS_ERROR_FMODE;  /* Bad file mode */
		goto error_out;
	}

	/* Change the file attribute to free */
	fib.fib_attr = SOS_FATTR_FREE;

	/* Update the directory entry. */
	rc = fs_swd_write_dent(ch, ioctx, dir_vn, &fib);
	if ( rc != 0 )
		goto error_out;

	/* Release blocks */
	rc = change_filesize_sword(ioctx, dir_vn, &fib, 0);
	if ( rc != 0 )
		goto error_out;

	rc = 0;

error_out:
	if ( resp != NULL )
		*resp = rc;

	return rc;
}

/** Open a file
    @param[in] fdp      The file descriptor
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    FS_VFS_FD_FLAG_O_ASC     Open/Create a ascii file
    FS_VFS_FD_FLAG_O_BIN     Open/Create a binary file
    @param[in]  pkt      The S-OS header operation packet.
    @param[in]  flags    The open flags
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO      I/O Error
    * SOS_ERROR_OFFLINE Device offline
    * SOS_ERROR_EXIST   File Already Exists
    * SOS_ERROR_NOENT   File not found
    * SOS_ERROR_NOSPC   Device Full (No free directory entry)
    * SOS_ERROR_RDONLY  Write proteced file
    * SOS_ERROR_SYNTAX  Invalid flags
 */
int
fops_open_sword(struct _fs_file_descriptor *fdp, const struct _sword_header_packet *pkt,
    fs_fd_flags flags, BYTE *resp){
	BYTE             res;
	struct _fs_vnode *vn;

	res = 0;

	vn = fdp->fd_vnode;

	/*
	 * Check arguments
	 */
	if ( FS_SWD_IS_OPEN_FLAGS_INVALID(pkt->hdr_attr, flags) )
		res = SOS_ERROR_SYNTAX;  /*  Invalid flags  */

	if ( !SOS_FATTR_IS_VALID(pkt->hdr_attr) ) /*  Invalid Attribute  */
		res = SOS_ERROR_SYNTAX;  /*  Invalid flags  */

	/*
	 * Check file attribute
	 */
	if ( SOS_FATTR_GET_FTYPE(pkt->hdr_attr)
	    != SOS_FATTR_GET_FTYPE(vn->vn_fib.fib_attr) ) /*  Attribute not match */
		res = SOS_ERROR_FMODE;  /* Bad file mode */

	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE)
	    && ( vn->vn_fib.fib_attr & SOS_FATTR_RDONLY ) )
		res = SOS_ERROR_RDONLY;   /* write protected file. */

	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE)
	    && ( vn->vn_fib.fib_attr & SOS_FATTR_DIR ) )
		res = SOS_ERROR_RDONLY;   /* Directory can not be opened to write. */

	if ( resp != NULL )
		*resp = res;

	return res;
}

/** Close a file
    @param[in] fdp      The file descriptor
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
 */
int
fops_close_sword(struct _fs_file_descriptor *fdp, BYTE *resp){

	return 0;
}

/** Read from a file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] dest   The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] rdsizp  The adress to store read bytes.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0                Success
    @retval    -1                Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fops_read_sword(struct _fs_file_descriptor *fdp,
    void *dest, size_t count, size_t *rdsizp, BYTE *resp){
	int                        rc;
	size_t                  rwsiz;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;

	sos_assert( fdp->fd_vnode != NULL );

	pos = &fdp->fd_pos;
	fib = &fdp->fd_vnode->vn_fib;

	rc = fs_swd_read_block(fdp->fd_ioctx, fib, pos->dp_pos, dest, count, &rwsiz);
	if ( rc != 0 )
		goto error_out;

	rc = 0;

error_out:
	if ( rdsizp != NULL )
		*rdsizp = rwsiz;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}

/** Write to a file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] src    The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the file.
    @param[out] wrsizp The adress to store written bytes.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
    @retval    -1      Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
    * SOS_ERROR_NOSPC  Device full
 */
int
fops_write_sword(struct _fs_file_descriptor *fdp, const void *src,
    size_t count, size_t *wrsizp, BYTE *resp){
	int                        rc;
	size_t                  rwsiz;
	size_t                  fixed;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;

	sos_assert( fdp->fd_vnode != NULL );

	pos = &fdp->fd_pos;
	fib = &fdp->fd_vnode->vn_fib;

	if ( ( pos->dp_pos == SOS_MAX_FILE_SIZE ) || ( count == 0 ) ){

		rc = 0;  /* Nothing to be done.  */
		goto error_out;
	}

	fixed = ( count > SOS_MAX_FILE_SIZE ) ? ( SOS_MAX_FILE_SIZE ) : ( count );
	rc = fs_swd_write_block(fdp->fd_ioctx, fib, pos->dp_pos, src, fixed, &rwsiz);
	if ( rc != 0 )
		goto error_out;
	/*
	 * Update file information block
	 */
	sos_assert( SOS_MAX_FILE_SIZE >= ( pos->dp_pos + rwsiz ) );

	fib->fib_size = STORAGE_FIB_FIX_SIZE( pos->dp_pos + rwsiz );

	if ( rc == 0 ) {

		/* Update the directory entry. */
		rc = fs_swd_write_dent(fib->fib_devltr, fdp->fd_ioctx,
		    fdp->fd_vnode, fib);
		if ( rc != 0 )
			goto error_out;
	}

	rc = 0;

error_out:
	if ( wrsizp != NULL )
		*wrsizp = rwsiz;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}

/** Truncate a file to a specified length
    @param[in]  fdp    The file descriptor to the file.
    @param[in]  offset The file length of the file to be truncated.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fops_truncate_sword(struct _fs_file_descriptor *fdp,
    fs_off_t offset, BYTE *resp){
	int                        rc;
	fs_off_t               newpos;
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	pos = &fdp->fd_pos;  /* position information for dirps/fatpos  */
	fib = &fdp->fd_vnode->vn_fib; /* file information block */
	offset=SOS_MIN( offset, SOS_MAX_FILE_SIZE -1);

	newpos = 0;
	if ( offset > 0 )
		newpos = offset;

	rc = change_filesize_sword(fdp->fd_ioctx, fdp->fd_vnode, fib, newpos);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}

/** Stat a file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] fib    The address to store the file information block.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
 */
int
fops_stat_sword(struct _fs_file_descriptor *fdp,
    struct _storage_fib *fib, BYTE *resp){

	return 0;
}

/** Reposition read/write file offset
    @param[in]  fdp     The file descriptor to the file.
    @param[in]  offset  The offset to reposition according to WHENCE.
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
fops_seek_sword(struct _fs_file_descriptor *fdp,
    fs_off_t offset, int whence, fs_off_t *new_posp, BYTE *resp){

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Rename the file
    @param[in] ch        The drive letter
    @param[in] ioctx     The current I/O context
    @param[in] src_vn    The directory v-node of the old name file.
    @param[in] oldname   The old file name
    @param[in] dest_vn   The directory v-node of the new name file.
    @param[in] newname   The new file name
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
*/
int
fops_rename_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const struct _fs_vnode *src_vn, const char *oldname,
    const struct _fs_vnode *dest_vn, const char *newname, BYTE *resp){
	int                             rc;
	struct _storage_fib        src_fib;
	struct _storage_fib       dest_fib;
	vfs_vnid                  old_vnid;
	vfs_vnid                  new_vnid;
	BYTE    old_swdname[SOS_FNAME_LEN];
	BYTE    new_swdname[SOS_FNAME_LEN];

	/* Get the filename of old name in SWORD representation. */
	rc = fs_unix2sword(oldname, &old_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Get the filename of new name in SWORD representation. */
	rc = fs_unix2sword(newname, &new_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a vnid for the file to be renamed. */
	rc = fs_swd_search_dent_by_name(ch, ioctx,
	    src_vn, &old_swdname[0], &old_vnid);
	if ( rc != 0 )
		goto error_out;

	/* Obtain the fib for the file to be renamed. */
	rc = fs_swd_search_fib_by_vnid(ch, ioctx, old_vnid, &src_fib);
	if ( rc != 0 )
		goto error_out;

	/* Set the new file information block up */
	memcpy(&dest_fib, &src_fib, sizeof(struct _storage_fib));
	memcpy(&dest_fib.fib_sword_name[0],&new_swdname[0],SOS_FNAME_LEN);

	/* Check that the new name is not used. */
	rc = fs_swd_search_dent_by_name(ch, ioctx, dest_vn, &new_swdname[0], &new_vnid);
	if ( rc == 0 ) {

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	if ( fs_swd_cmp_directory(src_vn, dest_vn) ) {

		/*
		 * If both of files are placed on the same directory,
		 * just write new entry.
		 */
		rc = fs_swd_write_dent(ch, ioctx, dest_vn, &dest_fib);
		if ( rc != 0 )
			goto error_out;
	} else {

		/*
		 * If both of files are placed on the different directories,
		 * write a new entry and then, remove the old entry.
		 */

		/* Search a free directory entry */
		rc = fs_swd_search_free_dent(ch, ioctx, dest_vn, &new_vnid);
		if ( rc != 0 )
			goto error_out;

		dest_fib.fib_vnid = new_vnid;  /* Update vnid */

		/* Write new entry */
		rc = fs_swd_write_dent(ch, ioctx, dest_vn, &dest_fib);
		if ( rc != 0 )
			goto error_out;

		/* free old entry */
		src_fib.fib_attr = SOS_FATTR_FREE;
		rc = fs_swd_write_dent(ch, ioctx, src_vn, &src_fib);
		if ( rc != 0 ) {

			dest_fib.fib_attr = SOS_FATTR_FREE;
			/* remove new entry */
			fs_swd_write_dent(ch, ioctx, dest_vn, &dest_fib);
		}
	}
error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}
/** Change file attribute
    @param[in]  vn    The v-node of the file.
    @param[in]  attr  The new file attribute.
    @param[out] resp  The address to store the return code for S-OS.
    @retval     0     Success
    @retval    -1     Error

 */
int
fops_set_attr_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    struct _fs_vnode *vn, const fs_attr attr, BYTE *resp){
	int                   rc;
	struct _storage_fib *fib;

	fib = &vn->vn_fib;

	fib->fib_attr = SOS_FATTR_GET_ALL_FTYPE(fib->fib_attr) | \
		SOS_FATTR_MASK_SOS_ATTR(attr);  /* Update attribute only */

	rc = fs_swd_write_dent(ch, ioctx, vn, fib); /* Update attribute */
	if ( rc != 0 )
		goto error_out;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}

/** Get file attribute
    @param[in]  vn    The v-node of the file.
    @param[out] attrp The address to store the file attribute.
    @param[out] resp  The address to store the return code for S-OS.
    @retval     0     Success
    @retval    -1     Error
 */
int
fops_get_attr_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    struct _fs_vnode *vn, fs_attr *attrp, BYTE *resp){
	int                   rc;
	struct _storage_fib *fib;

	fib = &vn->vn_fib;

	*attrp = fib->fib_attr;  /* Get attribute */

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Open directory
    @param[out] dir     The pointer to the directory stream.
    @param[out] resp    The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -1      Error
    @retval     EINVAL  Invalid whence
    @retval     ENXIO   The new position exceeded the file size
 */
int
fops_opendir_sword(struct _fs_dir_stream *dir, BYTE *resp){
	struct _fs_file_descriptor *fdp;
	struct _storage_disk_pos   *pos;

	fdp = dir->dir_fd;  /* File descriptor */
	pos = &fdp->fd_pos;  /* Position information */

	pos->dp_private = NULL; /* Init private information */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** close the directory
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
 */
int
fops_closedir_sword(struct _fs_dir_stream *dir, BYTE *resp){
	struct _fs_file_descriptor *fdp;
	struct _storage_disk_pos   *pos;

	fdp = dir->dir_fd;  /* File descriptor */
	pos = &fdp->fd_pos;  /* Position information */
	pos->dp_private = NULL; /* Clear private information */

	return 0;
}

/** Read the directory
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[out] fibp   The address to store the file information block
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
    @retval    -1      Error
    @retval     SOS_ERROR_IO    I/O Error
    @retval     SOS_ERROR_NOENT File not found (the end of the directory entry table )
    @remark     This function is responsible for setting the dir_pos member of
    the dir_pos structured variable in DIR and filling the FIB.
    Other members in the dir_pos should be set by the caller.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
    The function returns the file information block at the current position
    indicated by the dir_pos member of the dir_pos structured variable in DIR.
 */
int
fops_readdir_sword(struct _fs_dir_stream *dir, struct _storage_fib *fibp,
    BYTE *resp){
	int                          rc;
	struct _storage_fib    *dir_fib;
	struct _storage_fib         fib;
	struct _fs_file_descriptor *fdp;
	struct _storage_disk_pos   *pos;
	fs_dirno                  dirno;

	fdp = dir->dir_fd;  /* File descriptor */
	pos = &fdp->fd_pos;  /* Position information */
	dir_fib = &fdp->fd_vnode->vn_fib; /* File Information Block of the directory */

	/*
	 * read current entry
	 */
	dirno = FS_SWD_OFF2DIRNO(pos->dp_pos); /* Get #DIRNO */
	if ( dirno >= SOS_DENTRY_NR ) {

		rc = SOS_ERROR_NOENT;  /* Reaches max DIRNO */
		goto error_out;
	}

	if ( fibp == NULL )
		goto update_pos;

	/*
	 * Fill the file information block
	 */
	rc = fs_swd_search_dent_by_dirno(dir_fib->fib_devltr, fdp->fd_ioctx,
	    fdp->fd_vnode, SOS_DIRNO_VAL(dirno), &fib);
	if ( rc != 0 )
		goto error_out;

	memcpy(fibp, &fib, sizeof(struct _storage_fib));

update_pos:
	/*
	 * Update positions
	 *
	 * @remark This function regards a directory as a binary file containing
	 * an array of directory entries, it sets dir_pos only.
	 */
	pos->dp_pos = FS_SWD_DIRNO2OFF(dirno + 1);  /* file position */
	if ( FS_SWD_OFF2DIRNO(pos->dp_pos) == SOS_DENTRY_NR ) {

		pos->dp_pos = 0;       /* Reset the position in fd. */
		rc = SOS_ERROR_NOENT;  /* Reaches max DIRNO         */
		goto error_out;
	}

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}

/** Set the position of the next fs_readdir() call in the directory stream.
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[in]  dirno  The position of the next fs_readdir() call
    It should be a value returned by a previous call to fs_telldir.
    @param[out] resp   The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -EINVAL Invalid dirno
    @retval     -ENXIO  The new position exceeded the SOS_DENTRY_NR.
    @remark     This function is responsible for setting the dir_pos member of
    the dir_pos structured variable in DIR and filling the FIB.
    Other members in the dir_pos should be set by the caller.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
 */
int
fops_seekdir_sword(struct _fs_dir_stream *dir, fs_dirno dirno, BYTE *resp){
	struct _fs_file_descriptor *fdp;
	struct _storage_disk_pos   *pos;

	fdp = dir->dir_fd;  /* File descriptor */
	pos = &fdp->fd_pos;  /* Position information */

	if ( 0 > dirno )
		return -EINVAL;  /* Invalid #DIRNO */

	if ( dirno > SOS_DENTRY_NR )
		return -ENXIO;   /* #DIRNO is out of range. */

	pos->dp_pos = FS_SWD_DIRNO2OFF(dirno);  /* set seek position */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Return current location in directory stream
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[out] dirnop The address to store current location in directory stream.
    It should be a value returned by a previous call to fs_telldir.
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
    @retval    -1      Error
    The responses from the function:
    * SOS_ERROR_INVAL  The current position does not point the position in the file.
    @details    This function regards a directory as a binary file containing
    an array of directory entries.
 */
int
fops_telldir_sword(const struct _fs_dir_stream *dir, fs_dirno *dirnop,
    BYTE *resp){
	const struct _fs_file_descriptor *fdp;
	const struct _storage_disk_pos   *pos;
	fs_dirno                        dirno;

	fdp = dir->dir_fd;  /* File descriptor */
	pos = &fdp->fd_pos;  /* Position information */

	if ( dirnop == NULL ) {  /* No need to return dirno */

		*resp = SOS_ECODE_VAL(0);  /* return code */
		return 0;
	}

	*dirnop = FS_SWD_OFF2DIRNO(pos->dp_pos);  /* current position */
	sos_assert( SOS_DENTRY_NR > *dirnop );

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

void
init_sword_filesystem(void){

	fs_vfs_init_file_manager(&sword_fsm);
	sword_fsm.fsm_name = FS_SWD_FSNAME;
	sword_fsm.fsm_fops = &sword_fops;
	fs_vfs_register_filesystem(&sword_fsm);
}
