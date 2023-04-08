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
 * File Information block
 */
#define STORAGE_FIB_SIZE_MAX   (0xffff)  /**< Max size of SIZE/DTADR/EXADR */

/** Fix the file size up
    @param[in] _v The size to fix.
    @return  fixed size.
 */
#define STORAGE_FIB_FIX_SIZE(_v) \
	( ( (_v) > STORAGE_FIB_SIZE_MAX ) ? (STORAGE_FIB_SIZE_MAX) : (_v) )

/** Fill the file information block on the directory entry
    @param[in] _fib    The pointer to the file information block
    @param[in] _dent   The directory entry to copy the FIB to
 */
#define STORAGE_FIB2DENT(_fib, _dent) do{				\
		*(BYTE *)( (BYTE *)(_dent) + SOS_FIB_OFF_ATTR ) =	\
			((struct _storage_fib *)(_fib))->fib_attr;	\
		*(WORD *)( (BYTE *)(_dent) + SOS_FIB_OFF_SIZE ) =	\
			bswap_word_host_to_z80(				\
				((struct _storage_fib *)(_fib))->fib_size); \
		*(WORD *)( (BYTE *)(_dent) + SOS_FIB_OFF_DTADR ) =	\
			bswap_word_host_to_z80(				\
				((struct _storage_fib *)(_fib))->fib_dtadr); \
		*(WORD *)( (BYTE *)(_dent) + SOS_FIB_OFF_EXADR ) =	\
			bswap_word_host_to_z80(				\
				((struct _storage_fib *)(_fib))->fib_exadr); \
		*(WORD *)( (BYTE *)(_dent) + SOS_FIB_OFF_CLS ) =	\
			bswap_word_host_to_z80(				\
				((struct _storage_fib *)(_fib))->fib_cls); \
		memcpy(( (BYTE *)(_dent) + SOS_FIB_OFF_FNAME ),		\
		    &((struct _storage_fib *)(_fib))->fib_sword_name[0], \
		    SOS_FNAME_LEN);					\
		memset((BYTE *)( (BYTE *)(_dent) + SOS_FIB_OFF_DATE ), 0x0, \
		    SOS_FIB_SIZE - SOS_FIB_OFF_DATE - sizeof(WORD));	\
	}while(0)

/** Fill the file information block
    @param[in] _fib    The pointer to the file information block
    @param[in] _ch     The device letter
    @param[in] _dirno  The #DIRNO of the file from the beginning of the directory entry
    @param[in] _dent   The directory entry to copy the FIB from
 */
#define STORAGE_FILL_FIB(_fib, _ch, _dirno, _dent) do{		\
		((struct _storage_fib *)(_fib))->fib_devltr = (_ch);	\
		((struct _storage_fib *)(_fib))->fib_attr =		\
			*( (BYTE *)(_dent) + SOS_FIB_OFF_ATTR );	\
		((struct _storage_fib *)(_fib))->fib_dirno = (_dirno);	\
		((struct _storage_fib *)(_fib))->fib_size =		\
			bswap_word_z80_to_host( *(WORD *)( (BYTE *)(_dent) \
				+ SOS_FIB_OFF_SIZE ) );			\
		((struct _storage_fib *)(_fib))->fib_dtadr =		\
			bswap_word_z80_to_host( *(WORD *)( (BYTE *)(_dent) \
				+ SOS_FIB_OFF_DTADR ) );		\
		((struct _storage_fib *)(_fib))->fib_exadr =		\
			bswap_word_z80_to_host( *(WORD *)( (BYTE *)(_dent) \
				+ SOS_FIB_OFF_EXADR ) );		\
		((struct _storage_fib *)(_fib))->fib_cls =		\
			bswap_word_z80_to_host( *(WORD *)( (BYTE *)(_dent) \
				+ SOS_FIB_OFF_CLS ) );			\
		memcpy(&((struct _storage_fib *)(_fib))->fib_sword_name[0], \
		    ( (BYTE *)(_dent) + SOS_FIB_OFF_FNAME ), SOS_FNAME_LEN); \
	}while(0)

/*
 * Foward declarations
 */
struct _storage_di_ops;
struct  _fs_fs_manager;

/** File Information Block of the file
 */
struct _storage_fib{
	sos_devltr               fib_devltr;  /**< Device letter     */
	fs_sword_attr              fib_attr;  /**< File attribute    */
	fs_sword_dirno            fib_dirno;  /**< DIRNO of the file */
	WORD                       fib_size;  /**< File size         */
	WORD                      fib_dtadr;  /**< File load address */
	WORD                      fib_exadr;  /**< File exec address */
	WORD                        fib_cls;  /**< The first cluster on a disk */
	BYTE  fib_sword_name[SOS_FNAME_LEN];  /**< SWORD file name (Not C string) */
};

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

/** Disk Image File
 */
struct _storage_disk_image{
	struct _storage_disk_pos     di_pos; /**< Position information               */
	struct _storage_manager *di_manager; /**< Device manager                     */
	struct  _fs_fs_manager	*di_filesys; /**< File system                        */
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

/** Storage manager
 */
struct _storage_manager{
	struct _list             sm_node;   /**< List node                          */
	int                   sm_use_cnt;   /**< Use count                          */
	const char              *sm_name;   /**< Storage manager name               */
	struct _storage_di_ops   *sm_ops;   /**< Pointer to disk image operations   */
	void                 *sm_private;   /**< Private information for the device */
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
int storage_mount_filesystem(const sos_devltr _ch, struct _fs_fs_manager *_fs_mgr);
int storage_unmount_filesystem(const sos_devltr _ch);
#endif  /*  _STORAGE_H  */
