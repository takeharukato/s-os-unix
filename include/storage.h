/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator storage emulation module header                    */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_STORAGE_H)
#define _STORAGE_H

#include "sim-type.h"
#include "sos.h"
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
 */
#define STORAGE_DEVLTR_IS_VALID(_ch)					\
	( STORAGE_DEVLTR_IS_DISK((_ch)) || STORAGE_DEVLTR_IS_TAPE((_ch)) )

/*
 * Foward declaration
 */
struct _storage_di_ops;

/** File Information Block of the file
 */
struct _storage_fib{
	sos_devltr                       ch;  /**< Device letter     */
	BYTE                       fib_attr;  /**< File attribute    */
	BYTE                        fib_rec;  /**< Directory Entry Record No on a disk */
	WORD                       fib_size;  /**< File size         */
	WORD                      fib_dtadr;  /**< File load address */
	WORD                      fib_exadr;  /**< File exec address */
	BYTE  fib_sword_name[SOS_FNAME_LEN];  /**< the file name in Sword */
	unsigned char        *fib_unix_name;  /**< unix file name    */
};

/** Position information
 */
struct _storage_disk_pos{
	BYTE                   dp_dirno;   /**< Current #DIRNO                    */
	BYTE                  dp_retpoi;   /**< Current RETPOI                    */
	WORD                     dp_pos;   /**< File or device position           */
};

/** Storage manager
 */
struct _storage_manager{
	struct _list            *sm_node;   /**< list node pointer                  */
	int                   sm_use_cnt;   /**< use count                          */
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

/** Directory entry structure
 */
struct _sword_dirent{
       sos_devltr ch;  /**< The device letter */
       WORD    dirps;  /**< The start of the directory entry record */
       WORD      cur;  /**< The number of current directory entry */
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
	int (*record_read)(const sos_devltr _ch, BYTE *_dest, const WORD _rec,
	    const WORD _count, WORD *_rdcntp);
	int (*record_write)(const sos_devltr _ch, const BYTE *_src, const WORD _rec,
	    const WORD _count, WORD *_wrcntp);
};

/** Disk image operations
 */
struct _storage_diops_table{
	struct _queue head;   /**< Queue head */
};

void storage_init(void);
int storage_mount_image(const sos_devltr _ch, const char *_fname);
int storage_unmount_image(const sos_devltr _ch);
int storage_get_image_info(const sos_devltr _ch, struct _storage_disk_image *_resp);
int storage_fib_read(const sos_devltr _ch, const BYTE _dirno, struct _storage_fib *_fib);
int storage_fib_write(const sos_devltr _ch, const BYTE _dirno,
    const struct _storage_fib *_fib);
int storage_seq_read(const sos_devltr _ch, BYTE *_dest, const WORD _len);
int storage_seq_write(const sos_devltr _ch, const BYTE *_src, const WORD _len);
int storage_record_read(const sos_devltr _ch, BYTE *_dest, const WORD _rec,
    const WORD _count, WORD *_rdcntp);
int storage_record_write(const sos_devltr _ch, const BYTE *_src, const WORD _rec,
    const WORD _count, WORD *_wrcntp);

#endif  /*  _STORAGE_H  */
