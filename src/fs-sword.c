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
change_filesize_sword(struct _fs_ioctx *ioctx, struct _storage_fib *fib,
    struct _storage_disk_pos *pos,
    fs_off_t newpos){
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

	rc = fs_swd_write_dent(fib->fib_devltr, ioctx, fib); /* write back */
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

/** look up v-node
    @param[in] ch         The drive letter
    @param[in] ioctx      The current I/O context
    @param[in] super      The file system specific super block information.
    @param[in] vnid       The v-node ID to find
    @param[in] vn         The address to store v-node
    @retval    0          Success
 */
int
fops_lookup_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
	    vfs_fs_super super, vfs_vnid vnid, struct _fs_vnode *vn){
	int                  rc;
	struct _storage_fib fib;

	if ( vnid == FS_SWD_ROOT_VNID ) {

		/*
		 * Set root v-node up
		 */
		memset(vn, 0x0, sizeof(struct _fs_vnode));
		vn->vn_id = 0;
		storage_init_fib(&vn->vn_fib);
	} else {

		/*
		 * Search v-node in the directory
		 */
		rc = fs_swd_search_fib_by_vnid(ch, ioctx, vnid, &fib);
		if ( rc != 0 )
			goto error_out;
		memcpy(&vn->vn_fib, &fib, sizeof(struct _storage_fib));
		vn->vn_id = vnid;
	}

	return 0;

error_out:
	return rc;
}

/** Create a file
    @param[in] ch       The drive letter
    @param[in] ioctx    The current I/O context
    @param[in] fname    The filename to open
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    FS_VFS_FD_FLAG_O_ASC     Open/Create a ascii file
    FS_VFS_FD_FLAG_O_BIN     Open/Create a binary file
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] vn       The v-node to store the file information block
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    *    SOS_ERROR_IO     I/O Error
    *    SOS_ERROR_EXIST  File Already Exists
    *    SOS_ERROR_NOSPC  Device Full (No free directory entry)
    *    SOS_ERROR_SYNTAX Invalid flags
 */
int
fops_creat_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const char *fname, fs_fd_flags flags, const struct _sword_header_packet *pkt,
    struct _fs_vnode *vn, BYTE *resp){
	int                           rc;
	vfs_vnid                    vnid;
	struct _storage_fib          fib;
	BYTE     swd_name[SOS_FNAME_LEN];

	if ( FS_SWD_IS_OPEN_FLAGS_INVALID(pkt->hdr_attr, flags) )
		return SOS_ERROR_SYNTAX;  /*  Invalid flags  */

	/*
	 * convert the filename which was inputted from the console to
	 * the sword filename format.
	 */
	rc = fs_unix2sword(fname, &swd_name[0], SOS_FNAME_LEN);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search file from directory entry.
	 */
	rc = fs_swd_search_dent_by_name(ch, ioctx, &swd_name[0], &vnid);
	if ( rc == 0 ) {

		if ( !( flags & FS_VFS_FD_FLAG_O_EXCL ) )
			goto exists_ok;  /* Already created */

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/*
	 * Search a free entry
	 */
	rc = fs_swd_search_free_dent(ch, ioctx, &vnid);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOSPC;
		goto error_out;
	}

	/*
	 * create the new file
	 */

	/*
	 * Set the file information block up
	 */
	fib.fib_devltr = ch;
	fib.fib_attr = SOS_FATTR_GET_FTYPE(pkt->hdr_attr);
	fib.fib_vnid = vnid;
	fib.fib_size = 0;
	fib.fib_dtadr = pkt->hdr_dtadr;
	fib.fib_exadr = pkt->hdr_exadr;
	fib.fib_cls = FS_SWD_CALC_FAT_ENT_AT_LAST_CLS(1); /* No FAT alloced */
	memcpy(&fib.fib_sword_name[0],&swd_name[0],SOS_FNAME_LEN);

	/*
	 * Set file type
	 */

	/* Update the directory entry. */
	rc = fs_swd_write_dent(ch, ioctx, &fib);
	if ( rc != 0 )
		goto error_out;
exists_ok:
	/*
	 * return file information block
	 */
	memcpy(&vn->vn_fib, &fib, sizeof(struct _storage_fib));

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}
/** Open a file
    @param[in] ch       The drive letter
    @param[in] ioctx    The current I/O context
    @param[in] fname    The filename to open
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    FS_VFS_FD_FLAG_O_ASC     Open/Create a ascii file
    FS_VFS_FD_FLAG_O_BIN     Open/Create a binary file
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] vn       The v-node to store the file information block
    @param[out] privatep The pointer to the pointer variable to store
    the private information
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_EXIST  File Already Exists
    * SOS_ERROR_NOENT  File not found
    * SOS_ERROR_NOSPC  Device Full (No free directory entry)
    * SOS_ERROR_RDONLY Write proteced file
    * SOS_ERROR_SYNTAX Invalid flags
 */
int
fops_open_sword(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const char *filepath, fs_fd_flags flags, const struct _sword_header_packet *pkt,
    struct _fs_vnode *vn, void **privatep, BYTE *resp){
	int                           rc;
	struct _storage_fib          fib;
	struct _fs_vnode             lvn;
	vfs_vnid                    vnid;
	BYTE                         res;
	BYTE     swd_name[SOS_FNAME_LEN];

	if ( FS_SWD_IS_OPEN_FLAGS_INVALID(pkt->hdr_attr, flags) ) {

		rc = SOS_ERROR_SYNTAX;  /*  Invalid flags  */
		goto error_out;
	}

	if ( flags & FS_VFS_FD_FLAG_MAY_WRITE ) {

		/*
		 * TODO: Handle READ ONLY device case
		 */
	}

	/*
	 * Create a file
	 */
	if ( flags & FS_VFS_FD_FLAG_O_CREAT ) {

		rc = fops_creat_sword(ch, ioctx, filepath, flags, pkt, &lvn, &res);
		if ( rc != 0 ) {

			rc = res;
			goto error_out;
		}

		/* Copy the file information block */
		memcpy(&fib, &lvn.vn_fib, sizeof(struct _storage_fib));

		goto set_private_out;
	}

	/*
	 * convert the filename which was inputted from the console to
	 * the sword filename format.
	 */
	rc = fs_unix2sword(filepath, &swd_name[0], SOS_FNAME_LEN);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search file from directory entry.
	 */
	rc = fs_swd_search_dent_by_name(ch, ioctx, &swd_name[0], &vnid);
	if ( rc != 0 )
		goto error_out;
	rc = fs_swd_search_fib_by_vnid(ch, ioctx, vnid, &fib);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Check file attribute
	 */
	if ( SOS_FATTR_GET_FTYPE(fib.fib_attr) != SOS_FATTR_GET_FTYPE(pkt->hdr_attr) ) {

		rc = SOS_ERROR_NOENT;  /* File attribute was not matched. */
		goto error_out;
	}

	/*
	 * Determine whether the file is read-only-file.
	 */
	if ( ( flags & FS_VFS_FD_FLAG_MAY_WRITE )
	    && ( fib.fib_attr & SOS_FATTR_RDONLY ) ) {

			rc = SOS_ERROR_RDONLY;  /* Permission denied */
			goto error_out;
	}

	/*
	 * return file information block
	 */
	memcpy(&vn->vn_fib, &fib, sizeof(struct _storage_fib));

	rc = 0;

set_private_out:
	if ( privatep != NULL )
		*privatep = NULL;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return (rc == 0) ? (0) : (-1);
}

/** Close the file
    @param[in] fdp  The file descriptor to close
    @param[out] resp     The address to store the return code for S-OS.
    @retval      0  Success
 */
int
fops_close_sword(struct _sword_file_descriptor *fdp,
    BYTE *resp){

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Read from the file
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
fops_read_sword(struct _sword_file_descriptor *fdp, void *dest, size_t count,
    size_t *rdsizp, BYTE *resp){
	int                        rc;
	size_t                  rwsiz;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;

	pos = &fdp->fd_pos;
	fib = &fdp->fd_fib;

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

/** Write to the file
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
fops_write_sword(struct _sword_file_descriptor *fdp, const void *src,
    size_t count, size_t *wrsizp, BYTE *resp){
	int                        rc;
	size_t                  rwsiz;
	size_t                  fixed;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;

	pos = &fdp->fd_pos;
	fib = &fdp->fd_fib;

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
		rc = fs_swd_write_dent(fib->fib_devltr, fdp->fd_ioctx, fib);
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

/** Stat the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] vn     The v-node to store the file information block
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_NOENT  Block not found
    * SOS_ERROR_BADFAT Invalid cluster chain
    * SOS_ERROR_NOSPC  Device full
 */
int
fops_stat_sword(struct _sword_file_descriptor *fdp,
    struct _fs_vnode *vn, BYTE *resp){

	/* Copy the file infomation block */
	memmove(&vn->vn_fib, &fdp->fd_fib, sizeof(struct _storage_fib));

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Reposition read/write file offset
    @param[in]  fdp     The file descriptor to the file.
    @param[in]  offset  The offset to reposition according to WHENCE
    excluding the size of the S-OS header.
    @param[in]  whence  The directive to reposition:
     FS_VFS_SEEK_SET The file offset is set to offset bytes.
     FS_VFS_SEEK_CUR The file offset is set to its current location plus offset bytes.
     FS_VFS_SEEK_END The file offset is set to the size of the file plus offset bytes.
    @param[out] new_posp The address to store the new position excluding the size
    of the S-OS header.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0                Success
    @retval    -EINVAL           Invalid whence
 */
int
fops_seek_sword(struct _sword_file_descriptor *fdp, fs_off_t offset,
    int whence, fs_off_t *new_posp, BYTE *resp){
	fs_off_t                  new;
	fs_off_t                  cur;
	fs_off_t                  off;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;

	pos = &fdp->fd_pos;  /* Position information */
	fib = &fdp->fd_fib;  /* File information block */

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
		return -EINVAL;
	}

	if ( 0 > ( cur + off ) )
		new = 0;
	else if ( off > ( SOS_MAX_FILE_SIZE - cur ) )
		new = SOS_MAX_FILE_SIZE;
	else
		new = cur + off;

	if ( new_posp != NULL )
		*new_posp = new;  /* return the new position */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
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
fops_truncate_sword(struct _sword_file_descriptor *fdp,
    fs_off_t offset, BYTE *resp){
	int                        rc;
	fs_off_t               newpos;
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	fib = &fdp->fd_fib;  /* file information block */
	pos = &fdp->fd_pos;  /* position information for dirps/fatpos  */

	offset=SOS_MIN( offset, SOS_MAX_FILE_SIZE -1);

	newpos = 0;
	if ( offset > 0 )
		newpos = offset;
	rc = change_filesize_sword(fdp->fd_ioctx, fib, pos, newpos);
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

/** Open directory
    @param[out] dir     The pointer to the DIR structure (directory stream).
    @param[out] resp    The address to store the return code for S-OS.
    @retval      0      Success
    @retval     -1      Error
    @retval     EINVAL  Invalid whence
    @retval     ENXIO   The new position exceeded the file size
    @remark     DIRP has been initialized by the caller and this function is
    responsible for setting the dir_pos member of the dir_pos structured
    variable in DIR  and the private information.
 */
int
fops_opendir_sword(const char *filepath, struct _sword_dir *dir, BYTE *resp){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	pos->dp_pos = 0;  /* Set to the first directory entry */
	pos->dp_private = NULL; /* Init private information */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Read the directory
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[out] vn     The v-node to store the file information block
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
fops_readdir_sword(struct _sword_dir *dir, struct _fs_vnode *vn, BYTE *resp){
	int                        rc;
	struct _storage_fib  *dir_fib;
	struct _fs_vnode          lvn;
	struct _storage_disk_pos *pos;
	fs_dirno                dirno;

	pos = &dir->dir_pos;  /* Position information */
	dir_fib = &dir->dir_fib;  /* File Information Block of the directory */

	/*
	 * read current entry
	 */
	dirno = FS_SWD_OFF2DIRNO(pos->dp_pos); /* Get #DIRNO */
	if ( dirno >= SOS_DENTRY_NR ) {

		rc = SOS_ERROR_NOENT;  /* Reaches max DIRNO */
		goto error_out;
	}

	/*
	 * Fill the file information block
	 */
	rc = fs_swd_search_dent_by_dirno(dir_fib->fib_devltr, dir->dir_ioctx,
	    SOS_DIRNO_VAL(dirno), &lvn.vn_fib);
	if ( rc != 0 )
		goto error_out;

	memcpy(&vn->vn_fib, &lvn.vn_fib, sizeof(struct _storage_fib));

	/*
	 * Update positions
	 *
	 * @remark This function regards a directory as a binary file containing
	 * an array of directory entries, it sets dir_pos only.
	 */
	pos->dp_pos = FS_SWD_DIRNO2OFF(dirno + 1);  /* file position */
	if ( FS_SWD_OFF2DIRNO(pos->dp_pos) == SOS_DENTRY_NR ) {

		pos->dp_pos = 0;       /* Reset the position in fd. */
		rc = SOS_ERROR_NOENT;  /* Reaches max DIRNO */
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
fops_seekdir_sword(struct _sword_dir *dir, fs_dirno dirno, BYTE *resp){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

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
fops_telldir_sword(const struct _sword_dir *dir,
    fs_dirno *dirnop, BYTE *resp){
	const struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	if ( dirnop == NULL )
		goto error_out;

	*dirnop = FS_SWD_OFF2DIRNO(pos->dp_pos);  /* current position */

	sos_assert( SOS_DENTRY_NR > *dirnop );

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ERROR_INVAL;  /* return code */

	return -1;
}

/** close the directory
    @param[in]  dir    The pointer to the DIR structure (directory stream).
    @param[out] resp   The address to store the return code for S-OS.
    @retval     0      Success
 */
int
fops_closedir_sword(struct _sword_dir *dir, BYTE *resp){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;  /* Position information */

	pos->dp_pos = 0;        /* Reset position */
	pos->dp_private = NULL; /* Clear private information */

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(0);  /* return code */

	return 0;
}

/** Change the name of a file
    @param[in]  dir      The pointer to the DIR structure (directory stream).
    @param[in]  oldpath  The filename to be changed
    @param[in]  newpath  The filename to change to.
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0        Success
    @retval    -1        Error
    The responses from the function:
    * SOS_ERROR_EXIST Newpath already exists
    * SOS_ERROR_NOENT Oldpath is not Found.
 */
int
fops_rename_sword(struct _sword_dir *dir, const char *oldpath,
    const char *newpath, BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib        old_fib;
	struct _storage_fib        new_fib;
	struct _storage_fib       *dir_fib;
	vfs_vnid                      vnid;
	BYTE    old_swdname[SOS_FNAME_LEN];
	BYTE    new_swdname[SOS_FNAME_LEN];

	pos     = &dir->dir_pos;  /* Position information */
	dir_fib = &dir->dir_fib;  /* File Information Block of the directory */

	/* Get the filename of oldpath in SWORD representation. */
	rc = fs_unix2sword(oldpath, &old_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to be renamed. */
	rc = fs_swd_search_dent_by_name(dir_fib->fib_devltr, dir->dir_ioctx,
	    &old_swdname[0], &vnid);
	if ( rc != 0 )
		goto error_out;
	rc = fs_swd_search_fib_by_vnid(dir_fib->fib_devltr, dir->dir_ioctx,
	    vnid, &old_fib);
	if ( rc != 0 )
		goto error_out;

	/* Get the filename of newpath in SWORD representation. */
	rc = fs_unix2sword(newpath, &new_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Confirm the new filename doesn't exist.
	 */
	rc = fs_swd_search_dent_by_name(dir_fib->fib_devltr, dir->dir_ioctx,
	    &new_swdname[0], &vnid);
	if ( rc == 0 ) {

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/* Change the file name */
	memcpy(&old_fib.fib_sword_name[0],&new_swdname[0],SOS_FNAME_LEN);

	/* Update the directory entry. */
	rc = fs_swd_write_dent(dir_fib->fib_devltr, dir->dir_ioctx, &old_fib);
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

/** Change permission of a file
    @param[in]  dir     The pointer to the DIR structure (directory stream).
    @param[in]  path    The filename of changing the permission of the file
    @param[in]  perm    The new permission
    @param[out] resp    The address to store the return code for S-OS.
    @retval     0       Success
    @retval    -1       Error
    The responses from the function:
    * SOS_ERROR_NOENT path is not Found.
 */
int
fops_chmod_sword(struct _sword_dir *dir, const char *path,
    const fs_perm perm, BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib            fib;
	struct _storage_fib       *dir_fib;
	vfs_vnid                      vnid;
	BYTE        swdname[SOS_FNAME_LEN];

	pos     = &dir->dir_pos;  /* Position information */
	dir_fib = &dir->dir_fib;  /* File Information Block of the directory */

	/* Get the filename of oldpath in SWORD representation. */
	rc = fs_unix2sword(path, &swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to be renamed. */
	rc = fs_swd_search_dent_by_name(dir_fib->fib_devltr, dir->dir_ioctx,
	    &swdname[0], &vnid);
	if ( rc != 0 )
		goto error_out;
	rc = fs_swd_search_fib_by_vnid(dir_fib->fib_devltr, dir->dir_ioctx,
	    vnid, &fib);
	if ( rc != 0 )
		goto error_out;

	/* Change the file permission */
	if ( perm & FS_PERM_WR )
		fib.fib_attr &= ~SOS_FATTR_RDONLY;  /* clear readonly bit */
	else
		fib.fib_attr |= SOS_FATTR_RDONLY;  /* set readonly bit */

	/* Update the directory entry. */
	rc = fs_swd_write_dent(dir_fib->fib_devltr, dir->dir_ioctx, &fib);
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

/** Unlink a file
    @param[in]  dir  The pointer to the DIR structure (directory stream).
    @param[in]  path The filename to unlink
    @param[out] resp The address to store the return code for S-OS.
    @retval     0    Success
    @retval    -1    Error
    The responses from the function:
    * SOS_ERROR_IO     I/O Error
    * SOS_ERROR_BADFAT Invalid cluster chain
 */
int
fops_unlink_sword(struct _sword_dir *dir,
    const char *path, BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib            fib;
	struct _storage_fib       *dir_fib;
	vfs_vnid                      vnid;
	BYTE        swdname[SOS_FNAME_LEN];

	pos = &dir->dir_pos;  /* Position information */
	dir_fib = &dir->dir_fib;  /* File Information Block of the directory */

	/* Get the filename of path in SWORD representation. */
	rc = fs_unix2sword(path, &swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to unlink. */
	rc = fs_swd_search_dent_by_name(dir_fib->fib_devltr, dir->dir_ioctx,
	    &swdname[0], &vnid);
	if ( rc != 0 )
		goto error_out;

	rc = fs_swd_search_fib_by_vnid(dir_fib->fib_devltr, dir->dir_ioctx,
	    vnid, &fib);
	if ( rc != 0 )
		goto error_out;

	/* Change the file attribute to free */
	fib.fib_attr = SOS_FATTR_FREE;

	/* Update the directory entry. */
	rc = fs_swd_write_dent(dir_fib->fib_devltr, dir->dir_ioctx, &fib);
	if ( rc != 0 )
		goto error_out;

	/* Release the file allocation table for the file excluding S-OS header.
	 * @remark The file might have a bad allocation table.
	 * We should free the file allocation table after modifying the directory entry
	 * because we should make the file invisible in such a situation.
	 */
	rc = change_filesize_sword(dir->dir_ioctx, &fib, &dir->dir_pos, 0);
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
