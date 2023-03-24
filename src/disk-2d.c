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
static int get_info_2dimg(const sos_devltr _ch, struct _storage_disk_image * _resp);
static int fib_read_2dimg(const sos_devltr _ch, const BYTE _dirno,
	    struct _storage_fib *_fib, struct _storage_disk_pos *_posp);
static int fib_write_2dimg(const sos_devltr _ch, const BYTE _dirno,
	    const struct _storage_fib *const _fib, struct _storage_disk_pos *_posp);
static int seq_read_2dimg(const sos_devltr _ch, BYTE *_dest,
	    const WORD _len, struct _storage_disk_pos *_posp);
static int seq_write_2dimg(const sos_devltr _ch, const BYTE *_src,
	    const WORD _len, struct _storage_disk_pos *_posp);
static int record_read_2dimg(const sos_devltr _ch, BYTE *_dest, const WORD _rec,
	    const WORD _count);
static int record_write_2dimg(const sos_devltr _ch, const BYTE *_src, const WORD _rec,
	    const WORD _count);

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
	NULL, //get_info_2dimg,
	NULL, //get_fib_2dimg,
	NULL, //set_fib_2dimg,
	NULL, //seq_read_2dimg,
	NULL, //seq_write_2dimg,
	NULL, //record_read_2dimg,
	NULL //record_write_2dimg
};

static struct _disk2d_private disk_2d_private;  /* Private information */

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

/** Initialize 2D disk image module
 */
void
storage_2dimg_init(void){

	init_private_info_2dimg();
}
