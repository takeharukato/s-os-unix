/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator - virtual file system path handling module         */
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

/** Obtains a reference to the v-node of the specified path (internal function)
   @param[in]  ch        Drive letter
   @param[in]  ioctx     I/O context
   @param[in]  path      Path string
   @param[out] outv      The address of the pointer variable to point the found v-node.
   @retval   0               Success
   @retval   SOS_ERROR_OFFLINE Device offline
   @retval   SOS_ERROR_IO      I/O Error
   @retval   SOS_ERROR_NOENT   File not found
 */
static int
path_to_vnode(sos_devltr ch, const struct _fs_ioctx *ioctx, const char *path,
    struct _fs_vnode **outv){
	int                           rc;
	int                          idx;
	char                          *p;
	char                     *next_p;
	char copypath[SOS_UNIX_PATH_MAX];
	vfs_vnid                    vnid;
	struct _fs_vnode         *curr_v;
	struct _fs_vnode         *next_v;
	struct _storage_fib     *cur_fib;
	size_t                   pathlen;

	if ( !STORAGE_DEVLTR_IS_DISK(ch) )
		return SOS_ERROR_OFFLINE;

	idx = STORAGE_DEVLTR2IDX(ch);

	/* Copy path of the file  */
	pathlen = strlen(path);
	strncpy(copypath, path, pathlen);
	copypath[pathlen] = '\0';
	copypath[SOS_UNIX_PATH_MAX - 1] = '\0';

	p = copypath;

	if ( *p == FS_VFS_PATH_DELIM ) { /* Absolute path  */

		for( p += 1 ; *p == FS_VFS_PATH_DELIM; ++p);  /* Skip contiguous '/'. */

		/*  Search from the root directory.  */
		curr_v = ioctx->ioc_root[idx];
	} else { /* Relative path  */

		curr_v = ioctx->ioc_cwd[idx]; /*  Search from current directory.  */
	}

	/*
	 * Search path
	 */
	for( ; ; ) {

		if ( *p == '\0' )  /* The end of the path string. */
			break;

		/*
		 *  Search for string terminator or path delimiter.
		 */
		for(next_p = p + 1;
		    ( *next_p != '\0' ) && ( *next_p != FS_VFS_PATH_DELIM );
		    ++next_p);

		if ( *next_p == FS_VFS_PATH_DELIM ) {

			*next_p = '\0'; /* Terminate the string. */

			/* Skip contiguous '/'. */
			for( next_p += 1; *next_p == FS_VFS_PATH_DELIM; ++next_p);
		}

		/*
		 * @remark P points an element in the path string.
		 */

		if ( !FS_FSMGR_FOP_IS_DEFINED(curr_v->vn_mnt->m_fs, fops_lookup) )
			goto error_out;  /* No lookup operation */

		/*
		 * Acquire v-node lock to prevent directory entries from
		 * modifying during filename lookup.
		 */
		FS_VFS_LOCK_VNODE(curr_v);

		/* Get v-node ID from a directory entry
		 */
		rc = curr_v->vn_mnt->m_fs->fsm_fops->fops_lookup(ch,
		    ioctx, curr_v, p, &vnid);
		if ( rc != 0 )
			goto error_out;  /* No lookup operation */

		rc = fs_vfs_get_vnode(ch, ioctx, vnid, &next_v);
		if ( rc != 0 )
			goto error_out;  /* lookup operation failed */

		/* Unlock v-node
		 */
		FS_VFS_UNLOCK_VNODE(curr_v);

		/*
		 * Process the next element.
		 */
		p = next_p;
		curr_v = next_v;
	}

	rc = 0;

	if ( outv != NULL ) {

		FS_VFS_LOCK_VNODE(curr_v);
		*outv = curr_v;
	}

error_out:
	return rc;
}

/** Obtains a reference to the directory v-node of the specified path (internal function)
   @param[in]  ch        Drive letter
   @param[in]  ioctx     I/O context
   @param[in]  path      Path string
   @param[out] outv      The address of the pointer variable to point the found v-node.
   @param[out] fname     The address to store file name part.
   @param[in]  fnamelen  The length of the buffer to store file name part.
   @retval   0                 Success
   @retval   SOS_ERROR_OFFLINE Device offline
   @retval   SOS_ERROR_IO      I/O Error
   @retval   SOS_ERROR_NOENT   File not found
 */
static int
path_to_dir_vnode(sos_devltr ch, const struct _fs_ioctx *ioctx, const char *path,
    struct _fs_vnode **outv, char *fname, size_t fnamelen){
	int                            rc;
	int                           idx;
	char                           *p;
	char  copypath[SOS_UNIX_PATH_MAX];
	size_t                    pathlen;

	if ( !STORAGE_DEVLTR_IS_DISK(ch) )
		return SOS_ERROR_OFFLINE;

	idx = STORAGE_DEVLTR2IDX(ch);

	/* Copy path of the file  */
	pathlen = strlen(path);
	strncpy(copypath, path, pathlen);
	copypath[pathlen] = '\0';
	copypath[SOS_UNIX_PATH_MAX - 1] = '\0';

	p = strrchr(copypath, FS_VFS_PATH_DELIM);  /* Last delimiter */

	if ( p == NULL ) {  /* Return current directory v-node */

		if ( ( outv != NULL ) && ( fname != NULL ) ) {

			FS_VFS_LOCK_VNODE(ioctx->ioc_cwd[idx]);
			*outv = ioctx->ioc_cwd[idx]; /*  current directory.  */
			strncpy(fname, copypath, fnamelen);
			fname[fnamelen - 1] = '\0';
		}
	} else { /* Return directory v-node */

		*p = '\0';
		rc = path_to_vnode(ch, ioctx, copypath, outv);
		if ( rc != 0 )
			goto error_out;
		strncpy(fname, p + 1, fnamelen);
		fname[fnamelen - 1] = '\0';
	}

	rc = 0;

error_out:
	return rc;
}

/** Obtains a reference to the v-node of the specified path (internal function)
   @param[in]  ch        Drive letter
   @param[in]  ioctx     I/O context
   @param[in]  path      Path string
   @param[out] outv      The address of the pointer variable to point the found v-node.
   @retval   0                 Success
   @retval   SOS_ERROR_OFFLINE Device offline
   @retval   SOS_ERROR_IO      I/O Error
   @retval   SOS_ERROR_NOENT   File not found
 */
int
fs_vfs_path_to_vnode(sos_devltr ch, const struct _fs_ioctx *ioctx, const char *path,
    struct _fs_vnode **outv){

	return path_to_vnode(ch, ioctx, path, outv);
}


/** Obtains a reference to the v-node of the specified path (internal function)
   @param[in]  ch        Drive letter
   @param[in]  ioctx     I/O context
   @param[in]  path      Path string
   @param[out] outv      The address of the pointer variable to point the found v-node.
   @param[out] fname     The address to store the file name part.
   @param[out] fnamelen  Length of the file name part.
   @retval   0                 Success
   @retval   SOS_ERROR_OFFLINE Device offline
   @retval   SOS_ERROR_IO      I/O Error
   @retval   SOS_ERROR_NOENT   File not found
 */
int
fs_vfs_path_to_dir_vnode(sos_devltr ch, const struct _fs_ioctx *ioctx, const char *path,
    struct _fs_vnode **outv, char *fname, size_t fnamelen){

	return path_to_dir_vnode(ch, ioctx, path, outv, fname, fnamelen);
}
