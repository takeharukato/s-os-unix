/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator storage emulation module                           */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "config.h"

#include "sim-type.h"
#include "misc.h"
#include "sos.h"
#include "storage.h"
#include "list.h"
#include "queue.h"

/*
 * Macros
 */

/*
 * storage index for tape device
 */
#define STORAGE_FIRST_CMT_IDX ( ( SOS_DL_RESV_MAX - SOS_DL_DRIVE_A ) + 1 )
#define STORAGE_DSKIMG_IDX_T  ( STORAGE_FIRST_CMT_IDX )
#define STORAGE_DSKIMG_IDX_S  ( STORAGE_FIRST_CMT_IDX + 1 )
#define STORAGE_DSKIMG_IDX_Q  ( STORAGE_FIRST_CMT_IDX + 2 )

/** Convert from a disk image index to the drive letter of a tape device
    @param[in] _idx disk image index of STORAGE array
    @return The drive letter of _IDX
 */
#define STORAGE_IDX2TAPE_DEVLTR(_idx)					\
	( (_idx) == STORAGE_DSKIMG_IDX_T ? SOS_DL_COM_CMT :		\
	    ( (_idx) == STORAGE_DSKIMG_IDX_S ? SOS_DL_MON_CMT : SOS_DL_QD ) )

/** Convert from a disk image index to a drive letter
    @param[in] _idx disk image index of STORAGE array
    @return The drive letter of _IDX
 */
#define STORAGE_IDX2DRVLTR(_idx)					\
	( ( ( (_idx) >= STORAGE_DSKIMG_IDX_A )				\
	    && ( STORAGE_DSKIMG_IDX_A >= (_idx) ) ) ?			\
	    ( (_idx) + SOS_DL_DRIVE_A ) : STORAGE_IDX2TAPEDEV_LTR((_idx)) )

/** Convert from a drive letter to the disk image index
    @param[in] _ch a device letter
    @return The disk image index of STORAGE array coresponding to _CH
 */
#define STORAGE_DEVLTR2IDX(_ch)						\
	( STORAGE_DEVLTR_IS_DISK(_ch) ? ( (_ch) - SOS_DL_DRIVE_A ) :	\
	    ( ( (_ch) == SOS_DL_COM_CMT ) ? STORAGE_DSKIMG_IDX_T :	\
		( ( (_ch) == SOS_DL_COM_CMT ) ? STORAGE_DSKIMG_IDX_S :	\
		    STORAGE_DSKIMG_IDX_Q ) ) )

/** Determine whether the storage handler is invalid
    @param[in] _inf A pointer for storage disk image sturcture
    @param[in] _hdlr The function to be checked
    @retval    True   The handler can not be called.
    @retval    False  The handler can be called.
 */
#define STORAGE_HANDLER_IS_INVALID(_inf,_hdlr)		\
	( ( (_inf)->di_manager == NULL ) ||		\
	    ( (_inf)->di_manager->sm_ops == NULL ) ||	\
	    ( (_inf)->di_manager->sm_ops->_hdlr == NULL ) )

/*
 * Variables
 */
/** Storage table (Storage mount table) */
static struct _storage_disk_image storage[STORAGE_NR];
/** Disk image operation table */
static struct _storage_diops_table diops_tbl = __QUEUE_INITIALIZER(&diops_tbl.head);

/*
 * Internal functions
 */

/** Initialize storage position information
    @param[in] dpp The pointer to storage position information
 */
static void
init_storage_position(struct _storage_disk_pos *dpp){

	sos_assert( dpp != NULL );
	dpp->dp_dirno = 0;
	dpp->dp_retpoi = 0;
	dpp->dp_pos = 0;
}

/** Determine whether a device letter is valid.
    @param[in] ch    the device letter of a device on SWORD
    @param[in] idxp  the address to store a device index.
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
 */
static int
check_device_letter_common(const sos_devltr ch, int *idxp){
	int  rc;
	int dev;
	int idx;

	/*
	 * Check arguments
	 */
	dev = toupper((int)ch);
	if ( !STORAGE_DEVLTR_IS_VALID(dev) ) {

		rc = ENODEV;  /* Invalid device letter */
		goto out;
	}

	if ( !STORAGE_DEVLTR_IS_STD_DISK(dev) && !STORAGE_DEVLTR_IS_TAPE(dev) ) {

		/* The device letter does not point to a supported device. */
		rc = EINVAL;
		goto out;
	}

	idx = STORAGE_DEVLTR2IDX(dev);  /* Get index */
	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	rc = 0;

	if ( idxp != NULL )
		*idxp = idx;
out:
	return rc;
}
/** Reset storage disk image information
    @param[out] inf storage disk image information
 */
static void
init_storage_disk_image_info(struct _storage_disk_image *inf){

	init_storage_position(&inf->di_pos);
	inf->di_manager = NULL;
	inf->di_private = NULL;
}

/*
 * Interface functions
 */

/** Initialize storages
 */
void
storage_init(void){
	int i;
	struct _storage_disk_image *inf;

	for(i = 0; STORAGE_NR > i; ++i) {

		/*
		 * Make all of the devices unmounted.
		 */
		inf = &storage[i];
		init_storage_disk_image_info(inf);
	}

}

/** Register a storage operation
    @param[in] ops   The pointer to the storage manager to register
    @retval    0     success
    @retval    EINVAL The manager might be linked.
    @retval    EBUSY The operation has already registered.
    @remark    Set the name of the operation manager to ops->sm_name
    before calling this function.
*/
int
register_storage_operation(struct _storage_manager *ops){
	int                       rc;
	struct _list            *itr;
	struct _storage_manager *mgr;

	if ( !list_not_linked(ops->sm_node) )
		return EINVAL;

	queue_for_each(itr, &diops_tbl.head){

		mgr = container_of(itr, struct _storage_manager, sm_node);
		if ( !strcmp(ops->sm_name, mgr->sm_name) ) {

			rc = EBUSY;  /* The operation has already been registered. */
			goto error_out;
		}
	}

	/*
	 * Register the operation
	 */
	queue_add(&diops_tbl.head, ops->sm_node);
	ops->sm_use_cnt = 0;  /* Reset mount count */

	return 0;

error_out:
	return rc;
}
/** Unregister a storage operation
    @param[in] name   The name of  the storage manager to unregister
    @retval    0      success
    @retval    EBUSY  Some image files, managed by the storage operation to remove,
    are mounted.
*/
int
unregister_storage_operation(const char *name){
	int                       rc;
	struct _list            *itr;
	struct _storage_manager *mgr;

	queue_for_each(itr, &diops_tbl.head){

		mgr = container_of(itr, struct _storage_manager, sm_node);
		if ( strcmp(name, mgr->sm_name) )
			continue;

		if ( mgr->sm_use_cnt > 0 ) {

			rc = EBUSY;  /* Some image files are mounted. */
			break;
		}

		/*
		 * Unregister the operation
		 */
		queue_del(&diops_tbl.head, mgr->sm_node);
		rc = 0;
		break;
	}

	return rc;
}

/** mount a storage image file
    @param[in] ch    the device letter of a device on SWORD
    @param[in] fname the file name (file path) of a storage image file.
    @retval  0 success
    @retval ENODEV No such device
    @retval ENXIO  The image file is not supported.
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval EBUSY  The device has already been mounted.
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_mount_image(const sos_devltr ch, const char *const fname){
	int                          rc;
	int                        type;
	int                         idx;
	void                   *private;
	struct _storage_disk_image *inf;
	struct _list               *itr;
	struct _storage_manager    *mgr;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */

	if ( inf->di_manager != NULL ) {

		rc = EBUSY;
		goto out;  /* The device has already been mounted. */
	}

	rc = ENOENT;  /* Assume no handler found */
	queue_for_each(itr, &diops_tbl.head){

		mgr = container_of(itr, struct _storage_manager, sm_node);

		private = NULL;
		rc = mgr->sm_ops->mount_image(ch, fname, &private);
		if ( rc != 0 )
			continue;
		++mgr->sm_use_cnt;     /* Inc use count */
		inf->di_manager = mgr; /* Set manager */
		inf->di_private = private;
		rc = 0;
		break;
	}

out:
	return rc;
}

/** unmount a storage image file
    @param[in] ch    the device letter of a device on SWORD
    @param[in] fname the file name (file path) of a storage image file.
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval EBUSY  The device is busy.
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_unmount_image(const sos_devltr ch){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */

	if ( inf->di_manager == NULL ) {

		rc = ENXIO;  /*  The device has not been mounted.  */
		goto out;
	}

	if ( STORAGE_HANDLER_IS_INVALID(inf, umount_image) ) {

		rc = ENOENT;  /*  The device is not supported.  */
		goto out;
	}

	rc = inf->di_manager->sm_ops->umount_image(ch);
	if ( rc == 0 ) {

		sos_assert( inf->di_manager->sm_use_cnt > 0 );
		--inf->di_manager->sm_use_cnt;     /* dec use count */
		init_storage_disk_image_info(inf);
	}
out:
	return rc;
}

/** Get storage image information
    @param[in] ch    the device letter of a device on SWORD
    @param[in] resp  the address to store storage disk image information.
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_get_image_info(const sos_devltr ch, struct _storage_disk_image *resp){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */
	if ( STORAGE_HANDLER_IS_INVALID(inf, get_image_info) ) {

		rc = ENOENT;
		goto out;
	}

	return inf->di_manager->sm_ops->get_image_info(ch, &inf->di_pos);
out:
	return rc;
}

/** Read a file information block
    @param[in] ch    the device letter of a device on SWORD
    @param[in] fib  the address to store a file information block
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval ENOSPC File not found
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_fib_read(const sos_devltr ch, const BYTE dirno, struct _storage_fib *fib){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */
	if ( STORAGE_HANDLER_IS_INVALID(inf, fib_read) ) {

		rc = ENOENT;
		goto out;
	}

	 return inf->di_manager->sm_ops->fib_read(ch, dirno, fib, &inf->di_pos);

out:
	return rc;
}

/** Write a file information block
    @param[in] ch    the device letter of a device on SWORD
    @param[in] fib  the address of the file information block
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval ENOSPC File not found
    @retval EROFS Read-only device
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_fib_write(const sos_devltr ch, const BYTE dirno,
    const struct _storage_fib *fib){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */
	if ( STORAGE_HANDLER_IS_INVALID(inf, fib_write) ) {

		rc = ENOENT;
		goto out;
	}

	return inf->di_manager->sm_ops->fib_write(ch, dirno, fib, &inf->di_pos);

out:
	return rc;
}

/** Sequential read from a storage
    @param[in]  ch    The device letter of a device on SWORD
    @param[out] dest  The destination address of the data from a storage
    @param[in]  len   Transfer size
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval ENOSPC File not found
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_seq_read(const sos_devltr ch, BYTE *dest, const WORD len){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */
	if ( STORAGE_HANDLER_IS_INVALID(inf, seq_read) ) {

		rc = ENOENT;
		goto out;
	}

	return inf->di_manager->sm_ops->seq_read(ch, dest, len, &inf->di_pos);

out:
	return rc;
}

/** Sequential write to a storage
    @param[in]  ch    The device letter of a device on SWORD
    @param[in]  src   The source address of the data to write
    @param[in]  len   Transfer size
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval ENOSPC File not found
    @retval EPERM  Operation not permitted
    @retval EROFS  Read-only device
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_seq_write(const sos_devltr ch, const BYTE *src, const WORD len){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */
	if ( STORAGE_HANDLER_IS_INVALID(inf, seq_write) ) {

		rc = ENOENT;
		goto out;
	}

	return inf->di_manager->sm_ops->seq_write(ch, src, len, &inf->di_pos);

out:
	return rc;
}

/** Read sectors from a disk
    @param[in]  ch    The device letter of a device on SWORD
    @param[out] dest  The destination address of the data from a storage
    @param[in]  rec   The start record number to read
    @param[in]  count The number how many records to read
    @param[out] rdcntp The number how many records read
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
int
storage_record_read(const sos_devltr ch, BYTE *dest, const WORD rec, const WORD count,
	WORD *rdcntp){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */
	if ( STORAGE_HANDLER_IS_INVALID(inf, record_read) ) {

		rc = ENOENT;
		goto out;
	}

	return inf->di_manager->sm_ops->record_read(ch, dest, rec, count, rdcntp);

out:
	return rc;
}

/** Write sectors to a disk
    @param[in]  ch    The device letter of a device on SWORD
    @param[in]  src   The source address of the data to write
    @param[in]  rec   The start record number to read
    @param[in]  count The number how many records to read
    @param[out] wrcntp The number how many records written
    @retval  0 success
    @retval ENODEV No such device
    @retval EINVAL The device letter is not supported.
    @retval ENOENT The device is not supported.
    @retval ENXIO  The device has not been mounted.
    @retval ENOSPC File not found
    @retval ENOTBLK Block device required
    @retval EPERM  Operation not permitted
    @retval EROFS  Read-only device
    @retval EIO    I/O Error.
    @retval ENOMEM Out of memory.
 */
int
storage_record_write(const sos_devltr ch, const BYTE *src, const WORD rec,
    const WORD count, WORD *wrcntp){
	int                          rc;
	int                         idx;
	struct _storage_disk_image *inf;

	/* Get device index */
	rc = check_device_letter_common(ch, &idx);
	if ( rc != 0 )
		return rc;

	sos_assert( (STORAGE_NR > idx) && ( idx >= 0 ) );

	inf = &storage[idx]; /* get disk image info */
	if ( STORAGE_HANDLER_IS_INVALID(inf, record_write) ) {

		rc = ENOENT;
		goto out;
	}

	return inf->di_manager->sm_ops->record_write(ch, src, rec, count, wrcntp);

out:
	return rc;
}
