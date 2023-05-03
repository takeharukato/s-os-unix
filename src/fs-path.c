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
   @retval   SOS_ERROR_NOSPC No more memory
 */
static int
path_to_vnode(sos_devltr ch, struct _fs_ioctx *ioctx, const char *path,
    struct _fs_vnode **outv){
	int                         rc;
	int                        idx;
	char                        *p;
	char                   *next_p;
	char                 *copypath;
	vfs_vnid                  vnid;
	struct _fs_vnode       *curr_v;
	struct _fs_vnode       *next_v;
	struct _storage_fib   *cur_fib;

	if ( !STORAGE_DEVLTR_IS_DISK(ch) )
		return SOS_ERROR_OFFLINE;

	copypath = strdup(path);  /* Copy path to the file  */
	if ( copypath == NULL ) {

		rc = SOS_ERROR_NOSPC;  /* No more memory */
		goto error_out;
	}

	idx = STORAGE_DEVLTR2IDX(ch);
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

		if ( *p == '\0' ) { /* The end of the path string. */

			rc = 0;

			if ( outv != NULL ) {

				FS_VFS_LOCK_VNODE(curr_v);
				*outv = curr_v;
			}
			break;
		}

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
			goto copy_path_free_out;  /* No lookup operation */

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
			goto copy_path_free_out;  /* No lookup operation */

		rc = fs_vfs_get_vnode(ch, ioctx, vnid, &next_v);
		if ( rc != 0 )
			goto copy_path_free_out;  /* lookup operation failed */

		/* Unlock v-node
		 */
		FS_VFS_UNLOCK_VNODE(curr_v);

		/*
		 * Process the next element.
		 */
		p = next_p;
		curr_v = next_v;
	}

copy_path_free_out:
	free(copypath);

error_out:
	return rc;
}

/** Obtains a reference to the v-node of the specified path (internal function)
   @param[in]  ch        Drive letter
   @param[in]  ioctx     I/O context
   @param[in]  path      Path string
   @param[out] outv      The address of the pointer variable to point the found v-node.
   @retval   0               Success
   @retval   SOS_ERROR_NOSPC No more memory
 */
int
fs_vfs_path_to_vnode(sos_devltr ch, struct _fs_ioctx *ioctx, const char *path,
    struct _fs_vnode **outv){

	return path_to_vnode(ch, ioctx, path, outv);
}
