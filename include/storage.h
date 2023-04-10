/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator storage emulation module header                    */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_STORAGE_H)
#define _STORAGE_H

#include "config.h"

#include "sim-type.h"
#include "sos.h"
#include "bswapmac.h"
#include "queue.h"

/*
 * Macros
 */

#define STORAGE_NR       (SOS_DEVICES_NR)     /**< The number of devices */

/*
 * File Extention
 */
#define STORAGE_DSKIMG_EXT_D88       "d88"    /**< D88 disk image */
#define STORAGE_DSKIMG_EXT_MZT1      "mzt"    /**< Japanese MZT tape image */
#define STORAGE_DSKIMG_EXT_MZT2      "m12"    /**< Japanese MZT tape image */
#define STORAGE_DSKIMG_EXT_MZT3      "mzf"    /**< Japanese MZT tape image */


/** Determine whether the drive letter points to a tape device.
    @param[in] _ch a device letter
    @retval TRUE a device letter is a tape device on SWORD
    @retval FALSE a device letter is NOT a tape device on SWORD
 */
#define STORAGE_DEVLTR_IS_TAPE(_ch)					\
	( ( (_ch) == SOS_DL_COM_CMT ) || \
	    ( (_ch) == SOS_DL_MON_CMT ) || \
	    ( (_ch) == SOS_DL_QD ) )

/** Convert from a tape device letter to a tape device switch value
    @param[in] _ch a device letter of a tape
    @retval 0 Common tape device
    @retval 1 Monitor tape device
    @retval 3 Quick Disk tape device
 */
#define STORAGE_DEVLTR2DVSW(_ch)					\
	( ( (_ch) == SOS_DL_COM_CMT ) ? ( SOS_TAPE_DVSW_COM ) :		\
	    ( ( (_ch) == SOS_DL_COM_CMT ) ? ( SOS_TAPE_DVSW_MON ) :	\
		SOS_TAPE_DVSW_QD) )

/** Convert from a tape device switch value to a tape device letter
    @param[in] _v  tape device switch value
    @retval 'T' Common tape device
    @retval 'S' Monitor tape device
    @retval 'Q' Quick Disk tape device
 */
#define STORAGE_DVSW2DEVLTR(_v)						\
	( ( (_v) == SOS_TAPE_DVSW_COM ) ? ( SOS_DL_COM_CMT ) :		\
	    ( ( (_v) == SOS_TAPE_DVSW_MON ) ? ( SOS_DL_COM_CMT ) :	\
		SOS_DL_QD ) )

/** Determine whether the drive letter points to a standard disk device on SWORD
    @param[in] _ch a device letter
    @retval TRUE a device letter is a standard disk device on SWORD
    @retval FALSE a device letter is NOT a standard disk device on SWORD
 */
#define STORAGE_DEVLTR_IS_STD_DISK(_ch)					\
	( ( SOS_DL_DRIVE_D >= (_ch) )					\
	    && ( (_ch) >= SOS_DL_DRIVE_A ) )

/** Determine whether the drive letter points to a disk device,
    including reserved devices on SWORD.
    @param[in] _ch a device letter
    @retval TRUE a device letter is a disk device on SWORD
    @retval FALSE a device letter is NOT a disk device on SWORD
 */
#define STORAGE_DEVLTR_IS_DISK(_ch)					\
	( ( SOS_DL_DRIVE_L >= (_ch) )					\
	    && ( (_ch) >= SOS_DL_DRIVE_A ) )

/** Determine whether the drive letter points to a device on SWORD.
    @param[in] _ch a device letter
    @retval TRUE a device letter is a device on SWORD
    @retval FALSE a device letter is NOT a device on SWORD
    @remark This macro is the core function of DEVCHK
 */
#define STORAGE_DEVLTR_IS_VALID(_ch)					\
	( STORAGE_DEVLTR_IS_DISK((_ch)) || STORAGE_DEVLTR_IS_TAPE((_ch)) )

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
    @param[in] _ch a drive letter
    @return The disk image index of STORAGE array coresponding to _CH
 */
#define STORAGE_DEVLTR2IDX(_ch)						\
	( STORAGE_DEVLTR_IS_DISK(_ch) ? ( (_ch) - SOS_DL_DRIVE_A ) :	\
	    ( ( (_ch) == SOS_DL_COM_CMT ) ? STORAGE_DSKIMG_IDX_T :	\
		( ( (_ch) == SOS_DL_COM_CMT ) ? STORAGE_DSKIMG_IDX_S :	\
		    STORAGE_DSKIMG_IDX_Q ) ) )

/** Determine whether the file system manager is valid
    @param[in] _mgr     The pointer to the file system manager
    @param[in] _name    The pointer to the file system name
    @param[in] _ops     The pointer to the file system operations
    @param[in] _private The pointer to the private information
 */
#define STORAGE_FILL_MGR(_mgr, _name, _ops, _private) do{	\
		list_init(&((struct _storage_manager *)(_mgr)->sm_node)); \
		(struct _storage_manager *)(_mgr)->sm_use_cnt = 0;	\
		(struct _storage_manager *)(_mgr)->sm_name = (_name);	\
		(struct _storage_manager *)(_mgr)->sm_ops = (_ops);	\
		(struct _storage_manager *)(_mgr)->sm_private = (_private); \
	}while(0)

/** Forward declarations
 */
struct _storage_fib;

/** Type definitions
 */

/** Position information
 */
struct _storage_disk_pos{
	WORD                   dp_dirps;   /**< First directory entry record      */
	WORD                  dp_fatpos;   /**< File allocation table record      */
	BYTE                   dp_dirno;   /**< Current #DIRNO                    */
	BYTE                  dp_retpoi;   /**< Current RETPOI                    */
	fs_off_t                 dp_pos;   /**< File or device position
					    * including S-OS header.
					    */
	void                *dp_private;   /**< Private information  */
};

/** Storage manager
 */
struct _storage_manager{
	struct _list             sm_node;   /**< List node                          */
	int                   sm_use_cnt;   /**< Use count                          */
	const char              *sm_name;   /**< Storage manager name               */
	struct _storage_di_ops   *sm_ops;   /**< Pointer to disk image operations   */
	void                 *sm_private;   /**< Private information for the device */
};

/** Disk Image File
 */
struct _storage_disk_image{
	struct _storage_disk_pos     di_pos; /**< Position information               */
	struct _storage_manager *di_manager; /**< Device manager                     */
	void                    *di_private; /**< Private information for the device */
};

/** Disk image operations
 */
struct _storage_di_ops{
	int (*mount_image)(const sos_devltr _ch, const char *_fname, void **_ref_priv);
	int (*umount_image)(const sos_devltr _ch);
	int (*get_image_info)(const sos_devltr _ch, struct _storage_disk_pos *_posp);
	int (*fib_read)(const sos_devltr _ch, const BYTE _dirno,
	    struct _storage_fib *_fib, struct _storage_disk_pos *_posp);
	int (*fib_write)(const sos_devltr _ch, const BYTE _dirno,
	    const struct _storage_fib *const _fib, struct _storage_disk_pos *_posp);
	int (*seq_read)(const sos_devltr _ch, BYTE *_dest,
	    const WORD _len, struct _storage_disk_pos *_posp);
	int (*seq_write)(const sos_devltr _ch, const BYTE *_src,
	    const WORD _len, struct _storage_disk_pos *_posp);
	int (*record_read)(const sos_devltr _ch, BYTE *_dest, const fs_rec _rec,
	    const size_t _count, size_t *_rdcntp);
	int (*record_write)(const sos_devltr _ch, const BYTE *_src, const fs_rec _rec,
	    const size_t _count, size_t *_wrcntp);
};

/** Disk image operations
 */
struct _storage_diops_table{
	struct _queue head;   /**< Queue head */
};

void storage_init(void);
int register_storage_operation(struct _storage_manager *_ops);
int unregister_storage_operation(const char *_name);
void storage_init_position(struct _storage_disk_pos *_dpp);
void storage_init_fib(struct _storage_fib *_fibp);
int storage_mount_image(const sos_devltr _ch, const char *_fname);
int storage_unmount_image(const sos_devltr _ch);
int storage_get_image_info(const sos_devltr _ch, struct _storage_disk_image *_resp);
int storage_fib_read(const sos_devltr _ch, const BYTE _dirno, struct _storage_fib *_fib);
int storage_fib_write(const sos_devltr _ch, const BYTE _dirno,
    const struct _storage_fib *_fib);
int storage_seq_read(const sos_devltr _ch, BYTE *_dest, const WORD _len);
int storage_seq_write(const sos_devltr _ch, const BYTE *_src, const WORD _len);
int storage_record_read(const sos_devltr _ch, BYTE *_dest, const fs_rec _rec,
    const size_t _count, size_t *_rdcntp);
int storage_record_write(const sos_devltr _ch, const BYTE *_src, const fs_rec _rec,
    const size_t _count, size_t *_wrcntp);
int storage_set_dirps(const sos_devltr _ch, const fs_dirps _dirps);
int storage_set_fatpos(const sos_devltr _ch, const fs_fatpos _fatpos);
int storage_get_dirps(const sos_devltr _ch, fs_dirps *_dirpsp);
int storage_get_fatpos(const sos_devltr _ch, fs_fatpos *_fatposp);
int storage_check_status(const sos_devltr _ch);
#endif  /*  _STORAGE_H  */
