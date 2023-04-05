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
#include "storage.h"
#include "fs-vfs.h"
#include "fs-sword.h"


/** Allocate new block on the disk.
    @param[in]  ch      The drive letter
    @param[in]  pos     The file offset position
    @param[in]  use_recs The used record numbers at the last cluster
    @param[out] blknop  The address to store the block number of the new block.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOSPC  Device full
  */
static int
alloc_newblock_sword(sos_devltr ch, fs_rec use_recs, fs_cls *blknop){
	int                                rc;
	int                                 i;
	fs_sword_fatent                 *clsp;
	fs_sword_fatent  fat[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/*
	 * Search a free cluster.
	 */
	for( i = 0, clsp = &fat[0]; SOS_FAT_NR > i; ++i, clsp = &fat[i]){

		if ( *clsp == SOS_FAT_ENT_FREE ) {

			/* alloc new cluster and fill used records */
			if ( use_recs > 0 )
				*clsp = FS_SWD_MAKE_CLS_END(use_recs);
			else
				*clsp = FS_SWD_MAKE_CLS_END(SOS_CLUSTER_RECS);
			goto found;
		}
	}

	rc = SOS_ERROR_NOSPC;  /* Device full */
	goto error_out;

found:
	sos_assert( FS_SWD_IS_END_CLS(*clsp) );
	sos_assert( &fat[i] == clsp );

	clear_block_sword(ch, i);  /* clear new block */

	rc = write_fat_sword(ch, &fat[0]);  /* write fat */
	if ( rc != 0 )
		goto error_out;

	if ( blknop != NULL )
		*blknop = i;  /* return the block number */

	return 0;

error_out:
	return rc;
}

/** Get the cluster number of the block from the file position of the file.
    @param[in]  ch       The drive letter.
    @param[in]  fib      The file information block of the file contains the block.
    @param[in]  blkpos   The file position where the block is placed at.
    @param[out] clsp     The address to store the cluster number in FAT.
    @param[out] cls_sizp The address to store the available size in the cluster.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
get_cluster_number_sword(sos_devltr ch, struct _storage_fib *fib,
    fs_off_t blkpos, int mode, fs_sword_fatent *clsp, size_t *cls_sizp){
	int                     rc;
	fs_sword_fatent        cls;
	fs_sword_fatent   prev_cls;
	fs_off_t               pos;
	fs_cls               blkno;
	fs_cls_off     blk_remains;
	fs_rec            use_recs;
	BYTE  fat[SOS_RECORD_SIZE];

	rc = read_fat_sword(ch, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/* How many clusters to read */
	pos = SOS_MIN(blkpos,SOS_MAX_FILE_SIZE);
	/* The beginning of the cluster at the newsiz in cluster. */
	blk_remains = SOS_CALC_ALIGN(pos, SOS_CLUSTER_SIZE) / SOS_CLUSTER_SIZE;

	/*
	 * Ensure the first cluster of the file in case of writing.
	 */
	if ( FS_SWD_IS_END_CLS(fib->fib_cls) && FS_SWD_GETBLK_TO_WRITE(mode) ) {

			rc = alloc_newblock_sword(ch, 1, &blkno);
			if ( rc != 0 )
				goto error_out;

			rc = read_fat_sword(ch, &fat[0]);  /* reload fat */
			if ( rc != 0 )
				goto error_out;

			fib->fib_cls = SOS_CLS_VAL(blkno); /* set first cluster */
	}

	cls = SOS_CLS_VAL(fib->fib_cls); /* Get the first cluster */

	/*
	 * Get the cluster number (FAT index).
	 */
	while( blk_remains > 0 ) {

		prev_cls = cls;  /* Previous FAT */
		cls = fat[cls];  /* Next FAT */
		--blk_remains;   /* update counter */

		if ( cls == SOS_FAT_ENT_FREE ) {  /* Free entry */

			rc = SOS_ERROR_BADFAT;  /* Invalid cluster chain */
			goto error_out;
		}
		if ( !FS_SWD_IS_END_CLS(cls) )   /* End of cluster */
			continue;    /* next cluster */
		/*
		 * End of the cluster
		 */
		if ( !FS_SWD_GETBLK_TO_WRITE(mode) ) {

			/* return the block not found error  */
			rc = SOS_ERROR_NOENT;
			goto error_out;
		}

		/*
		 * allocate new block
		 */
		if ( blk_remains > 0 ) /* use all cluster */
			use_recs = SOS_REC_VAL(SOS_CLUSTER_RECS);
		else
			use_recs = FS_SWD_CALS_RECS_OF_LAST_CLS(pos);

		/* allocate a new block */
		rc = alloc_newblock_sword(ch, use_recs, &blkno);
		if ( rc != 0 )
			goto error_out;

		rc = read_fat_sword(ch, &fat[0]);   /* reload fat */
		if ( rc != 0 )
			goto error_out;

		fat[prev_cls] = SOS_CLS_VAL(blkno); /* make cluster chain */
		cls = fat[prev_cls];

		rc = write_fat_sword(ch, &fat[0]);  /* write fat */
		if ( rc != 0 )
			goto error_out;
	}

	sos_assert( !FS_SWD_IS_END_CLS(cls) );

	/*
	 * extends records in the cluster if it is needed.
	 */
	rc = read_fat_sword(ch, &fat[0]);   /* reload fat */
	if ( rc != 0 )
		goto error_out;

	if ( FS_SWD_IS_END_CLS(fat[cls]) && FS_SWD_GETBLK_TO_WRITE(mode) ) {

		use_recs = FS_SWD_CALS_RECS_OF_LAST_CLS(pos);
		if ( use_recs > FS_SWD_FAT_END_CLS_RECS(fat[cls]) ) {

			fat[cls] = FS_SWD_MAKE_CLS_END(use_recs); /* Update */

			rc = write_fat_sword(ch, &fat[0]);  /* write fat */
			if ( rc != 0 )
				goto error_out;
		}
	}

	if ( clsp != NULL )
		*clsp = cls;

	if ( cls_sizp != NULL ) {

		/* Return the available size in the cluster */
		if ( FS_SWD_IS_END_CLS(fat[cls]) )
			*cls_sizp = FS_SWD_SIZE_OF_LAST_CLUSTER(fat[cls]);
		else
			*cls_sizp = SOS_CLUSTER_SIZE;
	}
	return 0;

error_out:
	return rc;
}

/** Release the block in the file.
    @param[in] ch     The drive letter
    @param[in] fib    The file information block of the file contains the block
    @param[in] size   The file length of the file to be truncated.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  File not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
static int
release_block_sword(sos_devltr ch, struct _storage_fib *fib, fs_off_t size) {
	int                                rc;
	int                            relcnt;
	fs_cls_off                     clsoff;
	fs_sword_fatent              prev_cls;
	fs_sword_fatent              next_cls;
	fs_sword_fatent          last_cls_ptr;
	fs_cls                            cls;
	fs_cls                        rel_cls;
	fs_rec                       used_rec;
	fs_off_t                       newsiz;
	fs_sword_fatpos                fatrec;
	fs_sword_fatent  fat[SOS_RECORD_SIZE];

	rc = get_dirps_and_fatpos(ch, NULL, &fatrec);
	if ( rc != 0 )
		goto error_out;

	rc = read_fat_sword(ch, &fat[0]);  /* read fat */
	if ( rc != 0 )
		goto error_out;

	/*
	 * Find the start block to release
	 */

	/* How many clusters to search */
	newsiz = SOS_MIN(size, SOS_MAX_FILE_SIZE);
	/* The beginning of the cluster at the newsiz in cluster. */
	clsoff = SOS_CALC_ALIGN(newsiz, SOS_CLUSTER_SIZE) / SOS_CLUSTER_SIZE;

	/*
	 * Search the cluster to free.
	 */
	for( cls = SOS_CLS_VAL(fib->fib_cls), prev_cls = cls, next_cls=fat[cls];
	     clsoff > 0; prev_cls = cls, cls = next_cls, next_cls = fat[next_cls],
		     --clsoff) {

		if ( cls == SOS_FAT_ENT_FREE ) {

			rc = SOS_ERROR_BADFAT;  /* Invalid cluster chain */
			goto error_out;
		}

		if ( FS_SWD_IS_END_CLS(cls) ) {

			/* return the block not found error  */
			rc = SOS_ERROR_NOENT;
			goto error_out;
		}
	}

	/* @remark We have not checked whether prev_cls is SOS_FAT_ENT_FREE yet
	 * when clsoff == 0. So we should check ( prev_cls == SOS_FAT_ENT_FREE ) with
	 * the following if clause.
	 */
	if ( FS_SWD_IS_END_CLS(prev_cls) || ( prev_cls == SOS_FAT_ENT_FREE ) )
		goto no_need_release;  /* no cluster allocated or invalid chain */

	/* Calculate used records */
	used_rec = FS_SWD_CALS_RECS_OF_LAST_CLS(newsiz);
	last_cls_ptr = fat[prev_cls]; /* save the first cluster number to release */
	/* Mark the end of cluster at the end of cluster after releasing */
	if  ( used_rec > 0 )
		fat[prev_cls] = FS_SWD_MAKE_CLS_END(used_rec);

	/*
	 * Release blocks
	 */
	if ( ( fib->fib_size - newsiz ) > SOS_CLUSTER_SIZE ) {

		for(rel_cls = last_cls_ptr,
			    relcnt= SOS_CALC_NEXT_ALIGN(fib->fib_size - newsiz,
				SOS_CLUSTER_SIZE);
		    relcnt > 0; --relcnt, rel_cls = next_cls){

			next_cls = fat[rel_cls];  /* remind next cluster */
			sos_assert( next_cls != SOS_FAT_ENT_FREE );

			fat[rel_cls] = SOS_FAT_ENT_FREE; /* free cluster */
			if ( FS_SWD_IS_END_CLS(next_cls) )
				break;
		}
	}

no_need_release:
	/*
	 * Handle the last cluster of the file
	 */
	if ( newsiz == 0 ) {

		if ( !FS_SWD_IS_END_CLS(fib->fib_cls) ) {

		    if ( fat[fib->fib_cls] != SOS_FAT_ENT_FREE )
			    fat[fib->fib_cls] = SOS_FAT_ENT_FREE;

		    fib->fib_cls = FS_SWD_MAKE_CLS_END(SOS_CLUSTER_RECS);
		}
	}

	rc = write_fat_sword(ch, &fat[0]);  /* write fat */
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/** Get the block in the file.
    @param[in]  ch       The drive letter
    @param[in]  fib      The file information block of the file contains the block
    @param[in]  pos      The file position where the block is placed at
    @param[in]  mode     The number to specify the behavior.
    * FS_SWD_GTBLK_RD_FLG Get block to read
    * FS_SWD_GTBLK_WR_FLG Get block to write
    @param[out] dest     The destination address of the buffer to write the contents of
    the block
    @param[in]  bufsiz   The size of the buffer to store the contents of the block
    @param[out] blkp     The address to store the cluster number of the block
    @param[out] cls_sizp The address to store the available size in the cluster.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_NOENT  Block not found
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
get_block_sword(sos_devltr ch, struct _storage_fib *fib, fs_off_t pos,
    int mode, BYTE *dest, size_t bufsiz, fs_cls *blkp, size_t *cls_sizp){
	int                     rc;
	fs_sword_fatent        cls;
	size_t            cls_size;

	/*
	 * Get the cluster number of POS
	 */
	rc = get_cluster_number_sword(ch, fib, pos, mode, &cls, &cls_size);
	if ( rc != 0 )
		goto error_out;

	rc = read_cluster_sword(ch, dest, cls, bufsiz, NULL);
	if ( rc != 0 )
		goto error_out;

	if ( blkp != NULL )
		*blkp = cls;  /* return the number of cluster */

	if ( cls_sizp != NULL )
		*cls_sizp = cls_size; /* return the available size in the cluster */

	/* update file size */
	fib->fib_size = \
		STORAGE_FIB_FIX_SIZE( SOS_MAX(fib->fib_size, pos % SOS_MAX_FILE_SIZE) );

	return 0;

error_out:
	return rc;
}

/** Put the block of the file.
    @param[in] ch     The drive letter
    @param[in] fib    The file information block of the file contains the block
    @param[in] src    The destination address of the block buffer to write
    @param[in] bufsiz The size of the buffer to store the contents of the block
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
    @retval    SOS_ERROR_NOSPC  Device full
 */
static int
put_block_sword(sos_devltr ch, struct _storage_fib *fib, fs_off_t pos,
    const void *src, size_t bufsiz){
	int                     rc;
	fs_sword_fatent        cls;

	/*
	 * Get the cluster number of POS
	 */
	rc = get_cluster_number_sword(ch, fib, pos, FS_SWD_GTBLK_WR_FLG, &cls, NULL);
	if ( rc != 0 )
		goto error_out;

	rc = write_cluster_sword(ch, src, cls, bufsiz, NULL);
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/** Clear remaining bytes in the cluster after the end of file.
    @param[in] ch               The drive letter
    @param[in] fib              The file information block of the file.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
static int
clear_remains_at_end(sos_devltr ch, struct _storage_fib *fib){
	int                        rc;
	size_t               cls_size;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	/* clear data from the current file size to the end of cluster */
	rc = get_block_sword(ch,
	    fib, fib->fib_size, FS_SWD_GTBLK_RD_FLG,
	    &clsbuf[0], SOS_CLUSTER_SIZE, NULL, &cls_size);
	if ( rc != 0 )
		goto error_out;

	sos_assert( cls_size >= fib->fib_size % SOS_CLUSTER_SIZE );

	/* Clear the newly allocated region. */
	memset(&clsbuf[0] + fib->fib_size % SOS_CLUSTER_SIZE,
	    0x0, cls_size - fib->fib_size % SOS_CLUSTER_SIZE);

	/* update culster */
	rc = put_block_sword(ch, fib, fib->fib_size, &clsbuf[0],
	    SOS_CLUSTER_SIZE);

	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/** Truncate a file to a specified length
    @param[in]  fib    The file information block of the file.
    @param[in]  pos    The file position information
    @param[in]  off    The file length of the file to be truncated.
    @retval    0                Success
    @retval    SOS_ERROR_IO     I/O Error
    @retval    SOS_ERROR_BADFAT Invalid cluster chain
 */
static int
change_filesize_sword(struct _storage_fib *fib, struct _storage_disk_pos *pos,
    fs_off_t off){
	int                        rc;
	fs_off_t               newsiz;
	fs_off_t              extends;
	size_t               cls_size;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	if ( ( 0 > off ) || ( off > SOS_MAX_FILE_SIZE ) )
		return SOS_ERROR_SYNTAX;

	newsiz = off;

	extends = newsiz - fib->fib_size;

	if ( 0 >= extends ) {

		/*
		 * Release file blocks
		 */
		rc = release_block_sword(fib->fib_devltr, fib, newsiz);
		if ( rc != 0 )
			goto error_out;
	} else {

		/* alloc new blocks to the newsize. */
		rc = get_block_sword(pos->dp_devltr,
		    fib, newsiz, FS_SWD_GTBLK_WR_FLG,
		    &clsbuf[0], SOS_CLUSTER_SIZE, NULL,
		    &cls_size);
		if ( rc != 0 )
			goto error_out;

		/* Clear remaining data after the end of file. */
		rc = clear_remains_at_end(pos->dp_devltr, fib);
		if ( rc != 0 )
			goto error_out;
	}

	/*
	 * update file information block
	 */
	fib->fib_size = STORAGE_FIB_FIX_SIZE( newsiz );  /* update size */

	rc = write_dent_sword(pos->dp_devltr, fib); /* write back */
	if ( rc != 0 )
		goto error_out;

	return 0;

error_out:
	return rc;
}

/*
 * File system operations
 */

/** Create a file
    @param[in] ch       The drive letter
    @param[in] fname    The filename to open
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    FS_VFS_FD_FLAG_O_ASC     Open/Create a ascii file
    FS_VFS_FD_FLAG_O_BIN     Open/Create a binary file
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] fibp     The address to store the file information block
    @param[out] resp     The address to store the return code for S-OS.
    @retval     0               Success
    @retval    -1               Error
    *    SOS_ERROR_IO     I/O Error
    *    SOS_ERROR_EXIST  File Already Exists
    *    SOS_ERROR_NOSPC  Device Full (No free directory entry)
    *    SOS_ERROR_SYNTAX Invalid flags
 */
int
fops_creat_sword(sos_devltr ch, const unsigned char *fname, fs_fd_flags flags,
    const struct _sword_header_packet *pkt, struct _storage_fib *fibp, BYTE *resp){
	int                           rc;
	fs_sword_dirno             dirno;
	struct _storage_fib          fib;
	BYTE     swd_name[SOS_FNAME_LEN];
	BYTE    clsbuf[SOS_CLUSTER_SIZE];

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
	rc = search_dent_sword(ch, &swd_name[0], &fib);
	if ( rc == 0 ) {

		if ( !( flags & FS_VFS_FD_FLAG_O_EXCL ) )
			goto exists_ok;  /* Already created */

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/*
	 * Search a free entry
	 */
	rc = search_free_dent_sword(ch, &dirno);
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
	fib.fib_dirno = dirno;
	fib.fib_size = 0;
	fib.fib_dtadr = pkt->hdr_dtadr;
	fib.fib_exadr = pkt->hdr_exadr;
	fib.fib_cls = FS_SWD_MAKE_CLS_END(SOS_CLUSTER_RECS); /* No FAT alloced */
	memcpy(&fib.fib_sword_name[0],&swd_name[0],SOS_FNAME_LEN);

	/*
	 * Set file type
	 */

	/* Update the directory entry. */
	rc = write_dent_sword(ch, &fib);
	if ( rc != 0 )
		goto error_out;
exists_ok:
	/*
	 * return file information block
	 */
	if ( fibp != NULL )
		memcpy(fibp, &fib, sizeof(struct _storage_fib));

	return 0;

error_out:
	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return -1;
}
/** Open a file
    @param[in] ch       The drive letter
    @param[in] fname    The filename to open
    @param[in] flags    The open flags
    FS_VFS_FD_FLAG_O_RDONLY  Read only open
    FS_VFS_FD_FLAG_O_WRONLY  Write only open
    FS_VFS_FD_FLAG_O_RDWR    Read/Write open
    FS_VFS_FD_FLAG_O_CREAT   Create a new file if the file does not exist.
    FS_VFS_FD_FLAG_O_ASC     Open/Create a ascii file
    FS_VFS_FD_FLAG_O_BIN     Open/Create a binary file
    @param[in]  pkt      The S-OS header operation packet.
    @param[out] fibp     The address to store the file information block
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
fops_open_sword(sos_devltr ch, const unsigned char *fname, fs_fd_flags flags,
    const struct _sword_header_packet *pkt, struct _storage_fib *fibp,
    void **privatep, BYTE *resp){
	int                           rc;
	struct _storage_fib          fib;
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

		rc = fops_creat_sword(ch, fname, flags, pkt, fibp, &res);
		if ( rc != 0 ) {

			rc = res;
			goto error_out;
		}
		goto set_private_out;
	}

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
	rc = search_dent_sword(ch, &swd_name[0], &fib);
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
	if ( fibp != NULL )
		memcpy(fibp, &fib, sizeof(struct _storage_fib));

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
fops_close_sword(struct _sword_file_descriptor *fdp, BYTE *resp){

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
	fs_off_t                  off;
	size_t                  rdcnt;
	size_t                 cpylen;
	size_t               cls_size;
	ssize_t               remains;
	void                      *dp;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	pos = &fdp->fd_pos;
	fib = &fdp->fd_fib;

	/* Adjust read size */
	FS_SWD_ADJUST_CONTERS(count, rdcnt, pos->dp_pos, remains);

	for(dp = dest, off = pos->dp_pos; remains > 0; ) {

		/*
		 * Copy data
		 */
		rc = get_block_sword(pos->dp_devltr, &fdp->fd_fib, off,
		    FS_SWD_GTBLK_RD_FLG, &clsbuf[0], SOS_CLUSTER_SIZE, NULL, &cls_size);

		if ( rc != 0 ) {

			/* Returns rdsize=0 with success due to the end of the file
			 * when the file position points the end of file.
			 */
			if ( rc == SOS_ERROR_NOENT )
				rc = 0;

			goto out;
		}

		/*
		 * Calculate the length to copy
		 */
		if ( remains > SOS_CLUSTER_SIZE )
			cpylen = SOS_MIN(SOS_CLUSTER_SIZE, cls_size);
		else
			cpylen = SOS_MIN(remains, cls_size);

		/* Copy the contents of the cluster to the buffer. */
		memcpy(dp, &clsbuf[0], cpylen);
		off += cpylen;
		dp += cpylen;
		remains -= cpylen;

		if ( SOS_CLUSTER_SIZE > cls_size )
			break;  /* EOF */
	}

	rc = 0;

out:
	if ( rdsizp != NULL )
		*rdsizp = rdcnt - remains;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}

/** Write to the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] src    The buffer to store read data.
    @param[in]  count  The counter how many bytes to read from the
    file.
    @param[out] wrsizp The adress to store written bytes.
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
fops_write_sword(struct _sword_file_descriptor *fdp, const void *src,
    size_t count, size_t *wrsizp, BYTE *resp){
	int                        rc;
	fs_off_t                  off;
	ssize_t               remains;
	size_t                  wrcnt;
	size_t                 endoff;
	const void                *sp;
	struct _storage_disk_pos *pos;
	struct _storage_fib      *fib;
	BYTE clsbuf[SOS_CLUSTER_SIZE];

	pos = &fdp->fd_pos;
	fib = &fdp->fd_fib;

	/* Adjust write size */
	FS_SWD_ADJUST_CONTERS(count, wrcnt, pos->dp_pos, remains);

	if ( pos->dp_pos == SOS_MAX_FILE_SIZE ) {

		rc = 0;  /* Nothing to be done.  */
		goto error_out;
	}

	for(sp = src, off = pos->dp_pos; remains > 0; ) {

		/*
		 * Write data to a block
		 */

		/* Calculate the end positon offset from the begging of the cluster */
		endoff = ( remains >= SOS_CLUSTER_SIZE ) ? (SOS_CLUSTER_SIZE - 1) :
			(remains - 1);

		/* Read the contents of the cluster. */
		rc = get_block_sword(pos->dp_devltr,
		    fib, off + endoff, FS_SWD_GTBLK_WR_FLG,
		    &clsbuf[0], SOS_CLUSTER_SIZE, NULL, NULL);
		if ( rc != 0 )
			goto update_fib;

		if ( remains >= SOS_CLUSTER_SIZE ) {

			/* Write the whole record. */
			memcpy(&clsbuf[0], sp, SOS_CLUSTER_SIZE);

			rc = put_block_sword(pos->dp_devltr,
			    fib, off, &clsbuf[0], SOS_CLUSTER_SIZE);
			if ( rc != 0 )
				goto update_fib;

			sp += SOS_CLUSTER_SIZE;
			off += SOS_CLUSTER_SIZE;
			remains -= SOS_CLUSTER_SIZE;
		} else {

			/* Read the record and write the remaining
			 * bytes from the beginning of the cluster.
			 */

			memcpy(&clsbuf[0], sp, remains);

			/* Write the remaining data from
			 * the beginning of the cluster.
			 */
			rc = put_block_sword(pos->dp_devltr, fib, off,
			    &clsbuf[0], remains);
			if ( rc != 0 )
				goto update_fib;

			sp += remains;
			off += remains;
			remains = 0;
		}
	}

	rc = 0;

update_fib:
	/*
	 * Update file information block
	 */
	sos_assert( SOS_MAX_FILE_SIZE >= ( pos->dp_pos + wrcnt - remains ) );
	sos_assert( ( pos->dp_pos + wrcnt - remains ) >= 0 );

	fib->fib_size = STORAGE_FIB_FIX_SIZE( pos->dp_pos + wrcnt - remains );

	if ( rc == 0 ) {

		/* Update the directory entry. */
		rc = write_dent_sword(pos->dp_devltr, fib);
		if ( rc != 0 )
			goto error_out;
	}
error_out:

	if ( wrsizp != NULL )
		*wrsizp = count - remains;

	if ( resp != NULL )
		*resp = SOS_ECODE_VAL(rc);  /* return code */

	return ( rc == 0 ) ? (0) : (-1);
}

/** Stat the file
    @param[in]  fdp    The file descriptor to the file.
    @param[out] fib    The buffer to store the file information block of the file.
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
fops_stat_sword(struct _sword_file_descriptor *fdp, struct _storage_fib *fib,
    BYTE *resp){

	/* Copy the file infomation block */
	memmove(fib, &fdp->fd_fib, sizeof(struct _storage_fib));

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
fops_seek_sword(struct _sword_file_descriptor *fdp, fs_off_t offset, int whence,
    fs_off_t *new_posp, BYTE *resp){
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
fops_truncate_sword(struct _sword_file_descriptor *fdp, fs_off_t offset, BYTE *resp){
	int                        rc;
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	fib = &fdp->fd_fib;  /* file information block */
	pos = &fdp->fd_pos;  /* position information for dirps/fatpos  */

	rc = change_filesize_sword(fib, pos, offset);
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
fops_opendir_sword(struct _sword_dir *dir, BYTE *resp){
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
    @param[out] fib    The pointer to the file information block.
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
fops_readdir_sword(struct _sword_dir *dir, struct _storage_fib *fib, BYTE *resp){
	int                        rc;
	struct _storage_disk_pos *pos;
	fs_dirno                dirno;
	fs_sword_dirno       swddirno;
	fs_rec                    rec;
	BYTE    dent[SOS_DENTRY_SIZE];

	pos = &dir->dir_pos;  /* Position information */

	/*
	 * read current entry
	 */
	dirno = FS_SWD_OFF2DIRNO(pos->dp_pos); /* Get #DIRNO */
	if ( dirno >= SOS_DENTRY_NR ) {

		rc = SOS_ERROR_NOENT;  /* Reaches max DIRNO */
		goto error_out;
	}

	swddirno = SOS_DIRNO_VAL(dirno);

	rc = read_dent_sword(pos->dp_devltr, swddirno, &rec,
	    &dent[0], SOS_DENTRY_SIZE);
	if ( rc != 0 )
		goto error_out;

	/*
	 * Fill the file information block
	 */
	if ( fib != NULL )
		STORAGE_FILL_FIB(fib, pos->dp_devltr, swddirno, &dent[0]);

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
    @param[in]  dirno  The position of the next fs_readdir() call
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
fops_telldir_sword(const struct _sword_dir *dir, fs_dirno *dirnop, BYTE *resp){
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
fops_rename_sword(struct _sword_dir *dir, const unsigned char *oldpath,
    const unsigned char *newpath, BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib        old_fib;
	struct _storage_fib        new_fib;
	BYTE    old_swdname[SOS_FNAME_LEN];
	BYTE    new_swdname[SOS_FNAME_LEN];

	pos = &dir->dir_pos;  /* Position information */

	/* Get the filename of oldpath in SWORD representation. */
	rc = fs_unix2sword(oldpath, &old_swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to be renamed. */
	rc = search_dent_sword(pos->dp_devltr, &old_swdname[0], &old_fib);
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
	rc = search_dent_sword(pos->dp_devltr, &new_swdname[0], &new_fib);
	if ( rc == 0 ) {

		rc = SOS_ERROR_EXIST;
		goto error_out;
	}

	/* Change the file name */
	memcpy(&old_fib.fib_sword_name[0],&new_swdname[0],SOS_FNAME_LEN);

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, &old_fib);
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
fops_chmod_sword(struct _sword_dir *dir, const unsigned char *path,
    const fs_perm perm, BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib            fib;
	BYTE        swdname[SOS_FNAME_LEN];

	pos = &dir->dir_pos;  /* Position information */

	/* Get the filename of oldpath in SWORD representation. */
	rc = fs_unix2sword(path, &swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to be renamed. */
	rc = search_dent_sword(pos->dp_devltr, &swdname[0], &fib);
	if ( rc != 0 )
		goto error_out;

	/* Change the file permission */
	if ( perm & FS_PERM_WR )
		fib.fib_attr &= ~SOS_FATTR_RDONLY;  /* clear readonly bit */
	else
		fib.fib_attr |= SOS_FATTR_RDONLY;  /* set readonly bit */

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, &fib);
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
fops_unlink_sword(struct _sword_dir *dir, const unsigned char *path,
    BYTE *resp){
	int                             rc;
	struct _storage_disk_pos      *pos;
	struct _storage_fib            fib;
	BYTE        swdname[SOS_FNAME_LEN];

	pos = &dir->dir_pos;  /* Position information */

	/* Get the filename of path in SWORD representation. */
	rc = fs_unix2sword(path, &swdname[0], SOS_FNAME_LEN);
	if ( rc != 0 ) {

		rc = SOS_ERROR_NOENT;
		goto error_out;
	}

	/* Obtain a directory entry for the file to unlink. */
	rc = search_dent_sword(pos->dp_devltr, &swdname[0], &fib);
	if ( rc != 0 )
		goto error_out;

	/* Change the file attribute to free */
	fib.fib_attr = SOS_FATTR_FREE;

	/* Update the directory entry. */
	rc = write_dent_sword(pos->dp_devltr, &fib);
	if ( rc != 0 )
		goto error_out;

	/* Release the file allocation table for the file excluding S-OS header.
	 * @remark The file might have a bad allocation table.
	 * We should free the file allocation table after modifying the directory entry
	 * because we should make the file invisible in such a situation.
	 */
	rc = change_filesize_sword(&fib, &dir->dir_pos, 0);
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
