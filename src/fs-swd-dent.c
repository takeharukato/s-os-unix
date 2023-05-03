/* -*- mode: C; coding:utf-8 -*- */
/*************************************************************************/
/*  SWORD Emulator sword file system module - directory entry operations */
/*                                                                       */
/*  Copyright 2023 Takeharu KATO                                         */
/*                                                                       */
/*************************************************************************/

#include <string.h>
#include <errno.h>

#include "config.h"

#include "freestanding.h"
#include "sim-type.h"
#include "misc.h"
#include "compat.h"
#include "storage.h"
#include "sos.h"
#include "fs-vfs.h"
#include "fs-sword.h"

#define FS_SWD_SEARCH_DENT_VNODE (0)
#define FS_SWD_SEARCH_DENT_DIRNO (1)

/** Get the cluster number from v-node ID
    @param[in] vnid v-node ID
    @return An index value of the file in the directory entry track.
    It starts from 1.
 */
#define FS_SWD_GET_VNID2IDX(_vnid)	( SOS_CLS_VAL( (_vnid) - 1 ) )

/** Get the v-node ID from the index value in the directory entry track.
    @param[in] _idx An index value of the file in the directory entry track.
    @return the lower 16bits value of the v-node ID.
    It starts from 1.
 */
#define FS_SWD_GET_IDX2VNID(_idx)	( SOS_CLS_VAL( (_idx) + 1 ) )

/** Calculate vnode from a directory v-node and v-node ID
    @param[in] _dir_vnode a directory v-node of the directory which contains the file
    @param[in] _vnid      a vnode ID of the file
    @return a v-node ID of the file
 */
#define FS_SWD_DENT_VNID(_dir_vnode, _vnid) \
	( ( ( (_dir_vnode)->vn_id ) << 16 ) | (_vnid) )

/** Get the directory's cluster number from v-node ID
    @param[in] vnid v-node ID
    @return the cluster number of the directory entry cluster.
 */
#define FS_SWD_GET_VNID2DIRCLS(_vnid)	( SOS_CLS_VAL( (_vnid) >> 16 ) )

/** Get a cluster number in a directory entry.
    @param[in] _dent a directory entry
    @return the first cluster number of the file
 */
#define FS_SWD_GET_CLS_IN_DENT(_dent) \
	(bswap_word_z80_to_host(*((WORD *)((BYTE *)(_dent) + SOS_FIB_OFF_CLS))))

/** Read the directory entry by #DIRNO
    @param[in]   ioctx     The current I/O context.
    @param[in]   dir_vnode The v-node of the directory
    @return     The record number of the directory entry.
 */
static fs_rec
calc_directory_entry_record(const struct _fs_ioctx *ioctx,
    const struct _fs_vnode *dir_vnode){
	fs_rec                 rec;

	/*
	 * Calculate record number
	 */
	if ( dir_vnode->vn_id == FS_SWD_ROOT_VNID )
		rec = ioctx->ioc_dirps;
	else
		rec = SOS_CLS2REC( FS_SWD_GET_VNID2DIRCLS(dir_vnode->vn_id) );

	return SOS_DIRPS_VAL( rec );
}

/** Read the directory entry by #DIRNO
    @param[in]   ch        The drive letter
    @param[in]   ioctx     The current I/O context.
    @param[in]   dir_vnode The v-node of the directory
    @param[in]   idx       The index of directory entry ( = lower word of v-node id ).
    @param[out]  dirnop    The address to store #DIRNO number of the directory entry
    @param[out]  dentp     The address to store the directory entry.
    @retval     0               Success
    @retval     SOS_ERROR_IO    I/O Error
    @retval     SOS_ERROR_NOENT File not found
 */
static int
read_one_dent_by_idx(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const struct _fs_vnode *dir_vnode, int idx, fs_dirno *dirnop, BYTE *dentp){
	int                      i;
	int                     rc;
	vfs_vnid               cur;
	fs_dirno             dirno;
	fs_rec                 rec;
	fs_attr               attr;
	size_t               rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	rec = calc_directory_entry_record(ioctx, dir_vnode); /* Get record number */
	for(cur = 0, dirno = 0;
	    SOS_DENTRY_NR > cur; ++rec) {

		/*
		 * Read each directory entry
		 */

		/* Read a directory entry record */
		rc = storage_record_read(ch, &buf[0], SOS_REC_VAL(rec), 1, &rdcnt);
		if ( rc != 0 )
			goto error_out;  /* Error */

		if ( rdcnt != 1 ) {

			rc = SOS_ERROR_IO;
			goto error_out;  /* I/O Error */
		}

		/*
		 * Search for the directory entry specified by #DIRNO
		 */
		for(i = 0, dent = &buf[0]; SOS_DENTRIES_PER_REC > i ;
		    ++i, ++cur, dent += SOS_DENTRY_SIZE ) {

			attr = SOS_FATTR_VAL( *( dent + SOS_FIB_OFF_ATTR ) );

			if ( attr == SOS_FATTR_FREE )
				continue; /* Free entry */

			if ( attr == SOS_FATTR_EODENT ) {

				rc = SOS_ERROR_NOENT; /* File not found */
				goto error_out;
			}

			if ( cur == idx ) {

				if ( dirnop != NULL )
					*dirnop = dirno;

				if ( dentp != NULL )
					memcpy(dentp, dent, SOS_DENTRY_SIZE);

				goto found;
			}
			++dirno;  /* Inc. #DIRNO */
		}
	}

	/*
	 * The end of directory entry was found.
	 */
	return SOS_ERROR_NOENT; /* File not found */

found:
	return  0;

error_out:
	return rc;
}

/** Search a file in the directory entry on the disk with #DIRNO.
    @param[in]  ch        The drive letter.
    @param[in]  ioctx     The current I/O context.
    @param[in]  dir_vnode The v-node of the directory
    @param[in]  dirno     The #DIRNO number of the directory entry to read.
    @param[out] fib       The destination address of the file information block.
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fs_swd_search_dent_by_dirno(sos_devltr ch, struct _fs_ioctx *ioctx,
    const struct _fs_vnode *dir_vnode, fs_dirno dirno, struct _storage_fib *fib){
	int                     rc;
	vfs_vnid               cur;
	vfs_vnid              vnid;
	fs_dirno         cur_dirno;
	BYTE dent[SOS_DENTRY_SIZE];

	for(cur = 0; SOS_DENTRY_NR > cur; ++ cur) {

		/*
		 * Read a directory entry
		 */
		rc = read_one_dent_by_idx(ch, ioctx, dir_vnode,
		    cur, &cur_dirno, &dent[0]);
		if ( rc != 0 )
			goto error_out;

		if ( cur_dirno == dirno )
			goto found;
	}

	/*
	 * The end of directory entry was found.
	 */
	return SOS_ERROR_NOENT; /* File not found */

found:

	/*
	 * Fill the file information block
	 */
	vnid = FS_SWD_DENT_VNID(dir_vnode, FS_SWD_GET_IDX2VNID(cur) );
	if ( fib != NULL )
		STORAGE_FILL_FIB(fib, ch, vnid, &dent[0]);

	return  0;

error_out:
	return rc;
}

/** Search a file in the directory entry on the disk.
    @param[in]  ch        The drive letter
    @param[in]  ioctx     The current I/O context.
    @param[in]  dir_vnode The v-node of the directory
    @param[in]  swd_name  The file name in SWORD(NOT C String)
    @param[out] vndip     The address to store v-node ID.
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fs_swd_search_dent_by_name(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const struct _fs_vnode *dir_vnode, const BYTE *swd_name, vfs_vnid *vnidp){
	int                     rc;
	vfs_vnid               cur;
	BYTE dent[SOS_DENTRY_SIZE];

	for(cur = 0; SOS_DENTRY_NR > cur; ++ cur) {

		/*
		 * Read a directory entry
		 */
		rc = read_one_dent_by_idx(ch, ioctx, dir_vnode, cur, NULL, &dent[0]);
		if ( rc != 0 )
			goto error_out;

		if ( memcmp(&dent[0] + SOS_FIB_OFF_FNAME, &swd_name[0],
			SOS_FNAME_NAMELEN) == 0 )
			goto found;  /* Found */
	}

error_out:
	return rc;

found:
	/*
	 * Return v-node ID.
	 */

	if ( vnidp != NULL )
		*vnidp = FS_SWD_DENT_VNID(dir_vnode, FS_SWD_GET_IDX2VNID(cur) );

	return 0;
}

/** Get the file information block in the directory entry.
    @param[in]  ch        The drive letter
    @param[in]  ioctx     The current I/O context.
    @param[in]  vnid      The v-node ID of the file
    @param[out] fib       The address to store the file information block.
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fs_swd_search_fib_by_vnid(sos_devltr ch, const struct _fs_ioctx *ioctx,
    vfs_vnid vnid, struct _storage_fib *fib){
	int                      i;
	int                     rc;
	vfs_vnid               cur;
	fs_rec                 rec;
	fs_attr               attr;
	size_t               rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	dent = &buf[0];

	/*
	 * Calculate record number
	 */
	if ( SOS_RESERVED_FAT_NR > FS_SWD_GET_VNID2DIRCLS(vnid) )
		rec = SOS_DIRPS_VAL( ioctx->ioc_dirps );
	else
		rec = SOS_DIRPS_VAL( SOS_CLS2REC( FS_SWD_GET_VNID2DIRCLS(vnid) ) );

	for(cur = 0; SOS_DENTRY_NR > cur; ++rec) {

		/*
		 * Read each directory entry
		 */

		/* Read a directory entry record */
		rc = storage_record_read(ch, &buf[0], SOS_REC_VAL(rec), 1, &rdcnt);
		if ( rc != 0 )
			goto error_out;  /* Error */

		if ( rdcnt != 1 ) {

			rc = SOS_ERROR_IO;
			goto error_out;  /* I/O Error */
		}

		/*
		 * Search for the directory entry specified by vnid
		 */
		for(i = 0, dent=&buf[0]; SOS_DENTRIES_PER_REC > i;
		    ++i, ++cur, dent += SOS_DENTRY_SIZE ) {

			attr = SOS_FATTR_VAL( *( dent + SOS_FIB_OFF_ATTR ) );

			if ( attr == SOS_FATTR_FREE )
				continue; /* Free entry */

			if ( attr == SOS_FATTR_EODENT ) {

				rc = SOS_ERROR_NOENT; /* File not found */
				goto error_out;
			}

			if ( cur == SOS_CLS_VAL( FS_SWD_GET_VNID2IDX(vnid) ) )
				goto found;
		}
	}

	/*
	 * The end of directory entry was found.
	 */
	return SOS_ERROR_NOENT; /* File not found */

found:
	if ( fib != NULL )
		STORAGE_FILL_FIB(fib, ch, vnid, dent);

	return  0;

error_out:
	return rc;
}

/** Search a free directory entry on the disk.
    @param[in]  ch        The drive letter
    @param[in]  ioctx     The current I/O context.
    @param[in]  dir_vnode The v-node of the directory
    @param[out] vnidp     The the address to store v-node ID of the found entry.
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOSPC Free entry not found
 */
int
fs_swd_search_free_dent(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const struct _fs_vnode *dir_vnode, vfs_vnid *vnidp){
	int                     rc;
	int                      i;
	fs_rec                 rec;
	int                    idx;
	fs_attr               attr;
	size_t               rdcnt;
	BYTE                 *dent;
	BYTE  buf[SOS_RECORD_SIZE];

	rec = calc_directory_entry_record(ioctx, dir_vnode); /* Get record number */

	/*
	 * Search a free entry
	 */
	for(idx = 0; SOS_DENTRY_NR > SOS_DIRNO_VAL(idx); ++rec) {

		/*
		 * Read a directory entry
		 */
		rc = storage_record_read(ch, &buf[0], rec, 1, &rdcnt);
		if ( rc != 0 )
			goto error_out;  /* Error */

		if ( rdcnt != 1 ) {

			rc = SOS_ERROR_IO;
			goto error_out;  /* I/O Error */
		}

		/*
		 * Search a free entry
		 */
		for(i = 0, dent = &buf[0]; SOS_DENTRIES_PER_REC > i ;
		    ++i, ++idx, dent += SOS_DENTRY_SIZE ) {

			attr = SOS_FATTR_VAL(*( dent + SOS_FIB_OFF_ATTR ));
			if ( ( attr == SOS_FATTR_FREE ) || ( attr == SOS_FATTR_EODENT ) )
				goto found; /* an entry was found */
		}
	}

	/*
	 * Reach the end of directory
	 */
	rc = SOS_ERROR_NOSPC; /* Device Full */
	goto error_out;

found:
	/*
	 * Return the vnid of the free entry.
	 */
	if ( vnidp != NULL )
		*vnidp = FS_SWD_DENT_VNID(dir_vnode, FS_SWD_GET_IDX2VNID(idx) );

	return 0;

error_out:
	return rc;
}

/** Write the directory entry to the disk.
    @param[in] ch         The drive letter
    @param[in] ioctx      The current I/O context.
    @param[in] dir_vnode  The v-node of the directory
    @param[in] fib        The address of the file information block
    @retval    0               Success
    @retval    SOS_ERROR_IO    I/O Error
    @retval    SOS_ERROR_NOENT File not found
 */
int
fs_swd_write_dent(sos_devltr ch, const struct _fs_ioctx *ioctx,
    const struct _fs_vnode *dir_vnode,  struct _storage_fib *fib){
	int                      rc;
	fs_rec                  rec;
	fs_dirno       dirno_offset;
	size_t                rwcnt;
	BYTE                  *dent;
	BYTE   buf[SOS_RECORD_SIZE];

	/* Calculate the first directory entry record */
	rec = calc_directory_entry_record(ioctx, dir_vnode);

	/* Add the offset of the record address to the directory entry  */
	rec += SOS_DIRNO_VAL( FS_SWD_GET_VNID2IDX(fib->fib_vnid) )
		/ SOS_DENTRIES_PER_REC;

	/*
	 * Read directory entry
	 */
	rc = storage_record_read(ch, &buf[0], SOS_REC_VAL(rec), 1, &rwcnt);
	if ( rc != 0 )
		goto error_out;  /* Error */

	if ( rwcnt != 1 ) {

		rc = SOS_ERROR_IO;
		goto error_out;  /* I/O Error */
	}

	/* Calculate dirno offset in the record */
	dirno_offset = SOS_DIRNO_VAL( FS_SWD_GET_VNID2IDX(fib->fib_vnid) )
		% SOS_DENTRIES_PER_REC;

	/* refer the directory entry to modify */
	dent = (BYTE *)&buf[0] +  FS_SWD_DIRNO2OFF( SOS_DIRNO_VAL(dirno_offset) );
	STORAGE_FIB2DENT(fib, dent); 	/* Modify the entry */

	/*
	 * Write a directory entry
	 */
	rc = storage_record_write(ch, &buf[0], rec, 1, &rwcnt);
	if ( rc != 0 )
		goto error_out;  /* Error */

	if ( rwcnt != 1 ) {

		rc = SOS_ERROR_IO;
		goto error_out;  /* I/O Error */
	}

	return 0;

error_out:
	return rc;
}
