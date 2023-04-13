/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator virtual file system module header                  */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_FS_VFS_H)
#define _FS_VFS_H

#include "freestanding.h"

#include "config.h"
#include "sim-type.h"
#include "list.h"
#include "storage.h"

/** I/O Direction
 */
#define FS_VFS_IO_DIR_RD   (0)   /* read */
#define FS_VFS_IO_DIR_WR   (1)   /* write */

/** Determine the direction of getting block.
    @param[in] _mod  The direction flag
    FS_IO_DIR_RD Get block to read
    FS_IO_DIR_WR Get block to write
    @retval TRUE  Get block to write
    @retval FALSE Get block to read
 */
#define FS_VFS_IODIR_WRITE(_mod) ( (_mod) & FS_VFS_IO_DIR_WR )

#define FS_VFS_PATH_DELIM '/'   /**< Path delimiter */

/** file decriptor flags
 */
#define FS_VFS_FD_FLAG_O_RDONLY   (0x0)     /**< ReadOnly   */
#define FS_VFS_FD_FLAG_O_WRONLY   (0x1)     /**< WriteOnly  */
#define FS_VFS_FD_FLAG_O_RDWR     (0x2)     /**< Read/Write */
#define FS_VFS_FD_FLAG_O_CREAT    (0x4)     /**< Create     */
#define FS_VFS_FD_FLAG_O_EXCL     (0x8)     /**< Exclusive  */
#define FS_VFS_FD_FLAG_SYS_OPENED  (1)  /**< The file is opened */
/** Write flags
 */
#define FS_VFS_FD_FLAG_MAY_WRITE					\
	( FS_VFS_FD_FLAG_O_WRONLY|FS_VFS_FD_FLAG_O_RDWR|FS_VFS_FD_FLAG_O_CREAT )

/** Seek
 */
#define FS_VFS_SEEK_SET	(0)	/**< Seek from beginning of file.  */
#define FS_VFS_SEEK_CUR	(1)	/**< Seek from current position.   */
#define FS_VFS_SEEK_END	(2)	/**< Seek from end of file.        */

/** Permission
 */
#define FS_PERM_RD    (1)    /**< readable */
#define FS_PERM_WR    (2)    /**< writable */
#define FS_PERM_EX    (4)    /**< execute */

/** File Information block
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


/** File Descriptor
 */
#define FS_PROC_FDTBL_NR    (1)  /**< Max file decriptors for a process */

/** Determine whether the file system manager is valid
    @param[in] _fs_mgr The pointer to the file system manager
    @retval    True   The handler can not be called.
    @retval    False  The handler can be called.
 */
#define FS_FSMGR_IS_VALID(_fs_mgr)			\
	( ( (_fs_mgr) != NULL )				\
	    && ( (_fs_mgr)->fsm_name != NULL )		\
	    && ( (_fs_mgr)->fsm_fops != NULL ) )

/** Determine whether the file operation is defined.
    @param[in] _fs_mgr The pointer to the file system manager
    @param[in] _fop    The pointer to the file system operations
    @retval    True    The handler is defined
    @retval    False   The handler is not defined
 */
#define FS_FSMGR_FOP_IS_DEFINED(_fs_mgr, _fop)		\
	( FS_FSMGR_IS_VALID( (_fs_mgr) )		\
	    && ( (_fs_mgr)->fsm_fops->_fop != NULL ) )

/** Fill the file system manager parameters
    @param[in] _fs_mgr The pointer to the file system manager
    @param[in] _name   The pointer to the file system name
    @param[in] _fops   The pointer to the file system operations
 */
#define FS_FILL_MGR(_fs_mgr, _name, _fops) do{		\
		(struct _fs_fs_manager *)(_fs_mgr)->fsm_name = (_name);	\
		(struct _fs_fs_manager *)(_fs_mgr)->fsm_fops = (_fops);	\
		list_init(&((struct _fs_fs_manager *)(_fs_mgr)->fsm_node)); \
	}while(0)

/** v-node
 */
#define FS_VFS_VNODE_FREE   (0x00)  /**< Free v-node  */
#define FS_VFS_VNODE_BUSY   (0x80)  /**< Busy v-node  */

/** Determine whether the v-node is busy
    @param[in] _v      The v-node to investigate.
    @retval    True    The v-node is busy.
    @retval    False   The v-node is not busy.
 */
#define FS_VFS_IS_VNODE_BUSY(_v) ( (_v)->vn_status & FS_VFS_VNODE_BUSY )

/** Lock a v-node
    @param[out] _v      The v-node to lock.
 */
#define FS_VFS_LOCK_VNODE(_v)		      \
	do{					      \
		(_v)->vn_status |= FS_VFS_VNODE_BUSY; \
	}while(0)

/** Unock a v-node
    @param[out] _v      The v-node to unlock.
 */
#define FS_VFS_UNLOCK_VNODE_(_v)		      \
	do{					      \
		(_v)->vn_status &= ~FS_VFS_VNODE_BUSY; \
	}while(0)

/** Determine whether the v-node is free
    @param[in] _v      The v-node to investigate
    @retval    True    The v-node is free.
    @retval    False   The v-node is not free.
 */
#define FS_VFS_IS_VNODE_FREE(_v) \
	( !FS_VFS_IS_VNODE_BUSY( (_v) ) && ( (_v)->vn_mnt == NULL )  )

/** The number of vnode caches
    ( root vnode + current dir vnode ) * devices + 8 free entry
*/
#define FS_VFS_VNODE_NR       ( SOS_DEVICES_NR * 2 + 8 )

/** Foward declarations
 */
struct _fs_ioctx;
struct _storage_di_ops;
struct _fs_mount;

/** Type definitions
 */
typedef uint16_t       fs_perm;  /**< permission bit map */
typedef uint16_t   fs_fd_flags;  /**< fd flags           */
typedef uint32_t vfs_mnt_flags;  /**< mount flags        */
typedef void     *vfs_fs_vnode;  /**< file system specific v-node */
typedef void     *vfs_fs_super;  /**< file system specific super block */
typedef uint32_t      vfs_vnid;  /**< v-node ID */
typedef int       vfs_vn_state;  /**< v-node status */

/** File Information Block of the file
 */
struct _storage_fib{
	sos_devltr               fib_devltr;  /**< Drive letter      */
	fs_attr                    fib_attr;  /**< File attribute    */
	fs_dirno                  fib_dirno;  /**< DIRNO of the file */
	WORD                       fib_size;  /**< File size         */
	WORD                      fib_dtadr;  /**< File load address */
	WORD                      fib_exadr;  /**< File exec address */
	WORD                        fib_cls;  /**< The first cluster on a disk */
	BYTE  fib_sword_name[SOS_FNAME_LEN];  /**< SWORD file name (Not C string) */
};

/** File descriptor
 */
struct _sword_file_descriptor{
	fs_fd_flags            fd_flags;  /**< Open     flags */
	fs_fd_flags         fd_sysflags;  /**< Internal flags */
	struct _storage_disk_pos fd_pos;  /**< Position Information */
	struct _storage_fib      fd_fib;  /**< File Information Block */
	struct _fs_ioctx      *fd_ioctx;  /**< I/O context */
	void                *fd_private;  /**< Private Information */
};

/** Directory stream
 */
struct _sword_dir{
	struct _storage_disk_pos dir_pos;  /**< Position Information */
	fs_fd_flags         dir_sysflags;  /**< Internal flags       */
	struct _storage_fib      dir_fib;  /**< File Information Block */
	struct _fs_ioctx      *dir_ioctx;  /**< I/O context */
	void                *dir_private;  /**< Private Information  */
};

/** Information packet relevant to the S-OS header operations.
 */
struct _sword_header_packet{
	BYTE  hdr_attr; /**< File attribute    */
	WORD hdr_dtadr; /**< File load address */
	WORD hdr_exadr; /**< File exec address */
};

/** vfs stat
 */
struct _fs_vfs_stat{
	fs_blk_num     sb_blk_nr;  /**< The block numbers which the device contains */
	fs_blk_num   sb_freeblks;  /**< The block numbers of free blocks */
};

/** vnode
 */
struct _fs_vnode{
	struct _list                 vn_node;  /**< list node                         */
	vfs_vnid                       vn_id;  /**< v-node ID                         */
	vfs_vn_state               vn_status;  /**< v-node status                     */
	int                       vn_use_cnt;  /**< Use count                         */
	struct _fs_mount             *vn_mnt;  /**< Mount point                       */
	struct _storage_fib           vn_fib;  /**< File Information Block            */
	vfs_fs_vnode                vn_vnode;  /**< File system specific v-node       */
	void                     *vn_private;  /**< Private information               */
};

/** mount point
 */
struct _fs_mount{
	sos_devltr                 m_devltr;  /**< mount device                      */
	struct _queue	           m_vnodes;  /**< v-nodes in this mount point       */
	struct _fs_fs_manager         *m_fs;  /**< File system                       */
	struct _fs_vnode            *m_root;  /**< v-node of the root directory      */
	vfs_fs_super                m_super;  /**< File system specific super block  */
	vfs_mnt_flags         m_mount_flags;  /**< mount flags                       */
};

/** I/O Context
 */
struct _fs_ioctx{
	/** The FIB of the root directory for each drive.   */
	struct _fs_vnode                  *ioc_root[STORAGE_NR];
	/** The FIB of the current directory for each drive. */
	struct _fs_vnode                   *ioc_cwd[STORAGE_NR];
	/** The file descriptor table */
	struct _sword_file_descriptor ioc_fds[FS_PROC_FDTBL_NR];
	/** Current #DIRPS */
	fs_dirps                                      ioc_dirps;
	/** Current #FATPOS */
	fs_fatpos                                    ioc_fatpos;
};

/** File operations
 */
struct _fs_fops{
	void *fops_private;   /**< Private Information */
	int (*fops_mount)(sos_devltr _ch, const void *_args,
	    struct _fs_ioctx *_ioctx, vfs_fs_super *_superp,
	    vfs_mnt_flags *_mnt_flagsp, struct _fs_vnode **_root_vnodep);
	int (*fops_unmount)(sos_devltr _ch, vfs_fs_super _super,
	    struct _fs_vnode *_root_vnode);
	int (*fops_lookup)(const struct _fs_ioctx *_ioctx,
	    vfs_fs_super _fs_super, vfs_vnid vnid, struct _fs_vnode *_vn);
	int (*fops_creat)(sos_devltr _ch, const char *_filepath,
	    fs_fd_flags _flags, const struct _sword_header_packet *_pkt,
	    struct _fs_vnode *_vnodep, BYTE *_resp);
	int (*fops_open)(sos_devltr _ch, const char *_fname,
	    fs_fd_flags _flags, const struct _sword_header_packet *_pkt,
	    struct _fs_vnode *_vnodep, void **_privatep, BYTE *_resp);
	int (*fops_close)(struct _sword_file_descriptor *_fdp,
	    BYTE *_resp);
	int (*fops_read)(struct _sword_file_descriptor *_fdp,
	    void *_dest, size_t _count, size_t *_rdsizp, BYTE *_resp);
	int (*fops_write)(struct _sword_file_descriptor *_fdp,
	    const void *_src, size_t _count, size_t *_wrsizp, BYTE *_resp);
	int (*fops_stat)(struct _sword_file_descriptor *_fdp,
	    struct _fs_vnode *_vnodep, BYTE *_resp);
	int (*fops_seek)(struct _sword_file_descriptor *_fdp,
	    fs_off_t _offset, int _whence, fs_off_t *_newposp, BYTE *_resp);
	int (*fops_truncate)(struct _sword_file_descriptor *_fdp,
	    fs_off_t _offset, BYTE *_resp);
	int (*fops_opendir)(struct _sword_dir *_dir, BYTE *_resp);
	int (*fops_readdir)(struct _sword_dir *_dir, struct _fs_vnode *_vnodep,
	    BYTE *_resp);
	int (*fops_seekdir)(struct _sword_dir *_dir, fs_dirno _dirno, BYTE *_resp);
	int (*fops_telldir)(const struct _sword_dir *_dir, fs_dirno *_dirnop,
	    BYTE *_resp);
	int (*fops_closedir)(struct _sword_dir *_dir, BYTE *_resp);
	int (*fops_rename)(struct _sword_dir *_dir, const char *_oldpath,
	    const char *_newpath, BYTE *_resp);
	int (*fops_chmod)(struct _sword_dir *_dir, const char *_path,
	    const fs_perm _perm, BYTE *_resp);
	int (*fops_unlink)(struct _sword_dir *_dir,
	    const char *_path, BYTE *_resp);
};

/** File system manager
 */
struct _fs_fs_manager{
	struct _list             fsm_node;   /**< List node                    */
	int                   fsm_use_cnt;   /**< Use count                    */
	const char              *fsm_name;   /**< File system name             */
	struct _fs_fops         *fsm_fops;   /**< Pointer to file operations   */
	void                 *fsm_private;   /**< Private information          */
};

/** File system table
 */
struct _fs_filesystem_table{
	struct _queue head;   /**< Queue head */
};

int vfs_vnode_get_free_vnode(struct _fs_vnode **_vnodep);

int fs_vfs_lookup_filesystem(const char *_name, struct _fs_fs_manager **_fsmp);
int fs_vfs_register_filesystem(struct _fs_fs_manager *_fsm_ops);
int fs_vfs_unregister_filesystem(const char *_name);

int fs_vfs_get_vnode(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    vfs_vnid _vnid, struct _fs_vnode **_vnodep);
int vfs_put_vnode(struct _fs_vnode *_vn);
int vfs_invalidate_vnode(struct _fs_vnode *_vn);
void fs_vfs_init_vnode_tbl(void);

int fs_vfs_mnt_search_vnode(sos_devltr _ch, const struct _fs_ioctx *_ioctx,
    vfs_vnid _vnid, struct _fs_vnode **_vnodep);
int fs_vfs_mnt_mount_filesystem(sos_devltr _ch, const char *_fs_name,
    const void *_args, struct _fs_ioctx *_ioctx);
int fs_vfs_mnt_unmount_filesystem(sos_devltr _ch, struct _fs_ioctx *_ioctx);
void fs_vfs_init_mount_tbl(void);
#endif  /*  _FS_VFS_H  */
