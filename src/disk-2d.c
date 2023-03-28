/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator 2D byte stream disk emulation module               */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include "config.h"

#include "freestanding.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "sim-type.h"
#include "misc.h"
#include "sos.h"
#include "storage.h"
#include "compat.h"
#include "disk-2d.h"

/*
 * Macros
 */
/** Determine whether the driver letter is valid
    @remark 2D disk images can be mounted on a standard disk.
 */
#define DISK_2D_DEVLTR_IS_VALID(_ch) (STORAGE_DEVLTR_IS_STD_DISK((_ch)))

/** Convert from a device letter to index of the disk image array.
    @remark 2D disk images can be mounted on a standard disk.
 */
#define DISK_2D_DEVLTR2IDX(_ch) ( (_ch) - SOS_DL_DRIVE_A )

#define DSKIMG_EXT_2D   ".2d"     /**< 2D logical byte stream image */

/*
 * Forward declarations
 */
static int mount_2dimg(const sos_devltr _ch, const char *_fname, void **_ref_priv);
static int umount_2dimg(const sos_devltr _ch);

static int get_image_info_2dimg(const sos_devltr _ch, struct _storage_disk_pos *_posp);
static int fib_read_2dimg(const sos_devltr _ch, const BYTE _dirno,
    struct _storage_fib *_fib, struct _storage_disk_pos *_pos);
static int fib_write_2dimg(const sos_devltr _ch, const BYTE _dirno,
    const struct _storage_fib *const _fib, struct _storage_disk_pos *_posp);
static int seq_read_2dimg(const sos_devltr _ch, BYTE *_dest,
	    const WORD _len, struct _storage_disk_pos *_posp);
static int seq_write_2dimg(const sos_devltr _ch, const BYTE *_src,
	    const WORD _len, struct _storage_disk_pos *_posp);
static int record_read_2dimg(const sos_devltr _ch, BYTE *_dest, const WORD _rec,
	    const WORD _count, WORD *_rdcntp);
static int record_write_2dimg(const sos_devltr _ch, const BYTE *_src, const WORD _rec,
	    const WORD _count, WORD *_wrcntp);

/*
 * Variables
 */

/** disk image operations
    @remark The following code does not intentionally use the designated initializer
    in C99 for old UNIX systems.
 */
static struct _storage_di_ops diops_2dimg={
	mount_2dimg,
	umount_2dimg,
	get_image_info_2dimg,
	fib_read_2dimg,
	fib_write_2dimg,
	seq_read_2dimg,
	seq_write_2dimg,
	record_read_2dimg,
	record_write_2dimg
};

static struct _disk2d_private disk_2d_private;  /* Private information */

static struct _storage_manager disk_2d_manager = {
	__LIST_INITIALIZER(&disk_2d_manager.sm_node),
	0,
	"2D",
	&diops_2dimg,
	&disk_2d_private
};

/*
 * Internal functions
 */

/** Initialize private information
 */
static void
init_private_info_2dimg(void){
	int                     i;
	struct _disk2d_image *img;

	for(i = 0; DISK_2D_IMAGES_NR > i; ++i) {

		img = &disk_2d_private.images[i];  /* Disk image */
		img->fd = -1;
		img->fname = NULL;
	}
}

/** mount a storage image file
    @param[in] ch        The device letter of a device on SWORD
    @param[in] fname     The file name (file path) of a storage image file.
    @param[out] ref_priv the poiner to the pointer variable for the private information.
    @retval  0 success
    @retval  ENOENT The device is not supported by this module
 */
static int
mount_2dimg(const sos_devltr ch, const char *fname, void **ref_priv){
	int                    rc;
	int                    fd;
	const char           *ext;
	int                   idx;
	struct _disk2d_image *img;

	if ( !DISK_2D_DEVLTR_IS_VALID(ch) )
		return ENOENT;  /* The device is not supported by this module */

	ext = refer_file_extention(fname); /* Get extention */
	if ( ( ext != NULL ) && ( strcasecmp(DSKIMG_EXT_2D, ext) ) )
		return ENOENT;  /* The device is not supported by this module */

	idx = DISK_2D_DEVLTR2IDX(ch);        /* Get index  */
	img = &disk_2d_private.images[idx];  /* Disk image */

	sos_assert( ( img->fd == -1 ) && ( img->fname == NULL ) );

	fd = open(fname, O_RDWR);
	if ( 0 > fd )
		return EIO;  /* The device is not supported by this module */

	img->fname = strdup(fname);  /* copy file name string  */
	if ( img->fname == NULL ) {

		rc = ENOMEM;   /* Out of memory */
		goto close_out;
	}

	img->fd = fd;

	return 0;

close_out:
	close(fd);
	return rc;
}

/** unmount a storage image file
    @param[in] ch        The device letter of a device on SWORD
    @retval  0 success
    @retval  ENOENT The device is not supported by this module
 */
static int
umount_2dimg(const sos_devltr ch){
	int                    rc;
	int                   idx;
	struct _disk2d_image *img;

	if ( !DISK_2D_DEVLTR_IS_VALID(ch) )
		return ENOENT;  /* The device is not supported by this module */


	idx = DISK_2D_DEVLTR2IDX(ch);        /* Get index  */
	img = &disk_2d_private.images[idx];  /* Disk image */

	sos_assert( ( img->fd >= 0 ) && ( img->fname != NULL ) );

	rc = close( img->fd );  /* close the image file */
	sos_assert( rc == 0 );

	free( img->fname );     /* Free file name */

	img->fd = -1;
	img->fname = NULL;

	return 0;
}

/** Get storage image information
    @param[in]  ch    the device letter of a device on SWORD
    @param[out] posp  the address to store storage position information.
    @retval  0 success
    @retval ENOENT The device is not supported.
 */
static int
get_image_info_2dimg(const sos_devltr ch, struct _storage_disk_pos *posp){
	int                    rc;
	int                   idx;
	struct _disk2d_image *img;

	if ( !DISK_2D_DEVLTR_IS_VALID(ch) )
		return ENOENT;  /* The device is not supported by this module */

	return 0;
}

/** Read a file information block
    @param[in] ch    the device letter of a device on SWORD
    @param[in] dirno the #DIRNO of the file
    @param[out] fib  the address to store a file information block
    @param[out] posp the address to store storage position information
    @retval  0 success
    @retval ENOENT The device is not supported.
 */
static int
fib_read_2dimg(const sos_devltr ch, const BYTE dirno,
    struct _storage_fib *fib, struct _storage_disk_pos *pos){

	/* The device is not supported because this device is not tape  */
	return ENOENT;
}

/** Read a file information block
    @param[in] ch    the device letter of a device on SWORD
    @param[in] dirno the #DIRNO of the file
    @param[out] fib  the address to store a file information block
    @param[out] posp the address to store storage position information
    @retval  0 success
    @retval ENOENT The device is not supported.
 */
static int
fib_write_2dimg(const sos_devltr ch, const BYTE dirno,
    const struct _storage_fib *const fib, struct _storage_disk_pos *posp){

	/* The device is not supported because this device is not tape  */
	return ENOENT;
}

/** Read sequential data
    @param[in]  ch    the device letter of a device on SWORD
    @param[out] dest  the destination address to transfer data to
    @param[in]  len   transfer length
    @param[out] posp the address to store storage position information
    @retval  0 success
    @retval ENOENT The device is not supported.
*/
static int
seq_read_2dimg(const sos_devltr ch, BYTE *dest,
    const WORD len, struct _storage_disk_pos *posp){

	/* The device is not supported because this device is not tape  */
	return ENOENT;
}

/** Write sequential data
    @param[in]  ch    the device letter of a device on SWORD
    @param[out] dest  the destination address to transfer data to
    @param[in]  len   transfer length
    @param[out] posp the address to store storage position information
    @retval  0 success
    @retval ENOENT The device is not supported.
 */
static int
seq_write_2dimg(const sos_devltr ch, const BYTE *src,
    const WORD len, struct _storage_disk_pos *posp){

	/* The device is not supported because this device is not tape  */
	return ENOENT;
}

/** Read sectors from a disk
    @param[in]  ch    The device letter of a device on SWORD
    @param[out] dest  The destination address of the data from a storage
    @param[in]  rec   The start record number to read
    @param[in]  count The number how many records to read
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval ENOSPC File not found
    @retval ENOTBLK Block device required
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
static int
record_read_2dimg(const sos_devltr ch, BYTE *dest, const WORD rec,
    const WORD count, WORD *rdcntp){
	int                     rc;
	int                    idx;
	struct _disk2d_image  *img;
	BYTE data[SOS_RECORD_SIZE];
	ssize_t                res;
	WORD               remains;
	off_t                  pos;
	void                   *dp;

	if ( !DISK_2D_DEVLTR_IS_VALID(ch) )
		return ENOENT;  /* The device is not supported by this module */


	idx = DISK_2D_DEVLTR2IDX(ch);        /* Get index  */
	img = &disk_2d_private.images[idx];  /* Disk image */

	/* For the case of the failure of the lseek,
	 * calculate the remaining record counts beforehand.
	 */
	remains = count;

	/*
	 * Seek record position
	 */
	pos = lseek(img->fd, rec * SOS_RECORD_SIZE, SEEK_SET);
	if ( pos != ( rec * SOS_RECORD_SIZE ) ) {

		rc = EIO;
		goto out;
	}

	/*
	 * Read records
	 */
	rc = 0;                              /* Assume success */

	for(dp = dest; remains > 0; --remains) { /* read records sequentially */


		res = read(img->fd, &data[0], SOS_RECORD_SIZE);
		if ( res !=  SOS_RECORD_SIZE ) {

			rc = EIO;
			break;
		}
		memcpy(dp, &data[0], SOS_RECORD_SIZE); /* copy one record */
		dp += SOS_RECORD_SIZE; /* next address */
	}

out:
	if ( rdcntp != NULL )
		*rdcntp = count - remains;  /* retuns read records */

	return rc;
}

/** Write sectors from a disk
    @param[in]  ch    The device letter of a device on SWORD
    @param[out] dest  The destination address of the data from a storage
    @param[in]  rec   The start record number to read
    @param[in]  count The number how many records to read
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval ENOSPC File not found
    @retval ENOTBLK Block device required
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
static int
record_write_2dimg(const sos_devltr ch, const BYTE *src, const WORD rec,
    const WORD count, WORD *wrcntp){
	int                     rc;
	int                    idx;
	struct _disk2d_image  *img;
	ssize_t                res;
	WORD               remains;
	off_t                  pos;
	void                   *sp;

	if ( !DISK_2D_DEVLTR_IS_VALID(ch) )
		return ENOENT;  /* The device is not supported by this module */


	idx = DISK_2D_DEVLTR2IDX(ch);        /* Get index  */
	img = &disk_2d_private.images[idx];  /* Disk image */

	/* For the case of the failure of the lseek,
	 * calculate the remaining record counts beforehand.
	 */
	remains = count;

	/*
	 * Seek record position
	 */
	pos = lseek(img->fd, SEEK_SET, rec * SOS_RECORD_SIZE);
	if ( pos != ( rec * SOS_RECORD_SIZE ) ) {

		rc = EIO;
		goto out;
	}

	/*
	 * Write records
	 */
	rc = 0;                              /* Assume success */

	for(sp = (char *)src; remains > 0; --remains) { /* write records sequentially */

		res = write(img->fd, sp, SOS_RECORD_SIZE);
		if ( res !=  SOS_RECORD_SIZE ) {

			rc = EIO;
			break;
		}

		sp += SOS_RECORD_SIZE; /* next address */
	}

out:
	if ( wrcntp != NULL )
		*wrcntp = count - remains;  /* retuns written records */

	return rc;
}

/** Initialize 2D disk image module
 */
void
storage_2dimg_init(void){

	init_private_info_2dimg();
	register_storage_operation(&disk_2d_manager);
}
