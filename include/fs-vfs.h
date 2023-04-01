/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator virtual file system module header                  */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_FS_VFS_H)
#define _FS_VFS_H

#include "config.h"
#include "sim-type.h"
#include "list.h"
#include "storage.h"

/** file decriptor flags
 */
#define FS_VFS_FD_FLAG_O_RDONLY   (0x0)     /**< ReadOnly   */
#define FS_VFS_FD_FLAG_O_WRONLY   (0x1)     /**< WriteOnly  */
#define FS_VFS_FD_FLAG_O_RDWR     (0x2)     /**< Read/Write */
#define FS_VFS_FD_FLAG_O_CREAT    (0x4)     /**< Create  */
#define FS_VFS_FD_FLAG_SYS_OPENED  (1)  /**< The file is opened */
/** Write flags
 */
#define FS_VFS_FD_FLAG_MAY_WRITE					\
	( FS_VFS_FD_FLAG_O_WRONLY|FS_VFS_FD_FLAG_O_RDWR|FS_VFS_FD_FLAG_O_CREAT )


/** seek
 */
#define FS_VFS_SEEK_SET	(0)	/**< Seek from beginning of file.  */
#define FS_VFS_SEEK_CUR	(1)	/**< Seek from current position.   */
#define FS_VFS_SEEK_END	(2)	/**< Seek from end of file.        */

/** Permission
 */
#define FS_PERM_RD    (1)    /**< readable */
#define FS_PERM_WR    (2)    /**< writable */
#define FS_PERM_EX    (4)    /**< execute */

typedef WORD fs_perm;   /** permission bit map */

/** File descriptor
 */
struct _sword_file_descriptor{
	WORD                   fd_flags;  /**< Open     flags */
	WORD                fd_sysflags;  /**< Internal flags */
	struct _storage_disk_pos fd_pos;  /**< Position Information */
	struct _storage_fib      fd_fib;  /**< File Information Block */
	void                *fd_private;  /**< Private Information */
};

/** Directory stream
 */
struct _sword_dir{
	struct _storage_disk_pos dir_pos;   /**< Position Information               */
	void                *dir_private;   /**< Private Information                */
};

/** Information packet relevant to the S-OS header operations.
 */
struct _sword_header_packet{
	BYTE  hdr_attr; /**< File attribute    */
	WORD hdr_dtadr; /**< File load address */
	WORD hdr_exadr; /**< File exec address */
};

/** File operations
 */
struct _fs_fops{
	void *fops_private;   /**< Private Information */
	int (*fops_creat)(sos_devltr _ch, const unsigned char *_filepath, WORD _flags,
	    const struct _sword_header_packet *_pkt,
	    struct _storage_fib *_fibp);
	int (*fops_open)(sos_devltr _ch, const unsigned char *_filepath, WORD _flags,
	    const struct _sword_header_packet *_pkt,
	    struct _storage_fib *_fibp, void **_privatep);
	int (*fops_close)(struct _sword_file_descriptor *_fdp);
	int (*fops_read)(struct _sword_file_descriptor *_fdp, void *_dest,
	    size_t _count, size_t *_rdsizp);
	int (*fops_write)(struct _sword_file_descriptor *_fdp, const void *_src,
	    size_t _count, size_t *_wrsizp);
	int (*fops_stat)(struct _sword_file_descriptor *_fdp,
	    struct _storage_fib *_fibp);
	int (*fops_seek)(struct _sword_file_descriptor *_fdp, fs_off_t _offset,
	    int _whence, fs_off_t *_newposp);
	int (*fops_truncate)(struct _sword_file_descriptor *_fdp, fs_off_t _offset);
	int (*fops_opendir)(struct _sword_dir *_dir);
	int (*fops_readdir)(struct _sword_dir *_dir, struct _storage_fib *_fibp);
	int (*fops_seekdir)(struct _sword_dir *_dir, fs_dirno _dirno);
	int (*fops_telldir)(const struct _sword_dir *_dir, fs_dirno *_dirnop);
	int (*fops_closedir)(struct _sword_dir *_dir);
	int (*fops_rename)(struct _sword_dir *_dir, const unsigned char *_oldpath,
	    const unsigned char *_newpath);
	int (*fops_chmod)(struct _sword_dir *_dir, const unsigned char *_path,
	    const fs_perm _perm);
	int (*fops_unlink)(struct _sword_dir *_dir, const unsigned char *_path);
};

/** Superblock
 */
struct _fs_super_block{
	fs_blk_num     sb_blk_nr;  /**< The block numbers which the device contains */
	fs_blk_num   sb_freeblks;  /**< The block numbers of free blocks */
	WORD            sb_dirps;  /**< The the first directory entry record */
	WORD           sb_fatpos;  /**< The allocation table record */
};

/** File system manager
 */
struct _fs_fs_manager{
	struct _list             fsm_node;   /**< List node                    */
	int                   fsm_use_cnt;   /**< Use count                    */
	const char              *fsm_name;   /**< File system name             */
	struct _fs_fops         *fsm_fops;   /**< Pointer to file operations   */
	void                 *fsm_private;   /**< Private information          */
	int (*fsm_fill_super)(struct _fs_super_block *_super); /**< fill super block  */
};

#endif  /*  _FS_VFS_H  */
