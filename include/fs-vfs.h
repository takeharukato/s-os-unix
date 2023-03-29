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
#define FS_VFS_FD_FLAG_O_RDONLY   (0)  /**< ReadOnly   */
#define FS_VFS_FD_FLAG_O_WRONLY   (1)  /**< WriteOnly  */
#define FS_VFS_FD_FLAG_O_RDWR     (2)  /**< Read/Write */

#define FS_VFS_FD_FLAG_SYS_OPENED (1)  /**< The file is opened */

/** seek
 */
#define FS_VFS_SEEK_SET	(0)	/* Seek from beginning of file.  */
#define FS_VFS_SEEK_CUR	(1)	/* Seek from current position.   */
#define FS_VFS_SEEK_END	(2)	/* Seek from end of file.        */

/** File descriptor
 */
struct _sword_file_descriptor{
	WORD                   fd_flags;  /**< Open     flags */
	WORD                fd_sysflags;  /**< Internal flags */
	struct _storage_disk_pos fd_pos;  /**< Position Information */
	struct _storage_fib      fd_fib;  /**< File Information Block */
	void                *fd_private;  /**< Private Information */
};

/** dir structure for fs_opendir
 */
struct _sword_dir{
	struct _storage_disk_pos dent_pos;   /**< Position Information               */
	void                *dent_private;   /**< Private Information                */
};

/** File system operations
 */
struct _fs_fops{
	struct _list           fops_node;   /**< List node                          */
	int                 fops_use_cnt;   /**< Use count                          */
	const char            *fops_name;   /**< File system name                   */
	void               *fops_private;   /**< Private Information                */
	int (*fops_open)(sos_devltr _ch, const char *_filepath,
	    struct _storage_fib *_fib, void **_privatep);
	int (*fops_close)(struct _sword_file_descriptor *_fdp);
	int (*fops_read)(struct _sword_file_descriptor *_fdp, void *_dest,
	    size_t _count, size_t *_rdsizp);
	int (*fops_write)(struct _sword_file_descriptor *_fdp, const void *_src,
	    size_t _count, size_t *_wrsizp);
	int (*fops_stat)(struct _sword_file_descriptor *_fdp,
	    struct _storage_fib *_fibp);
	int (*fops_seek)(struct _sword_file_descriptor *_fdp, fs_off_t _offset,
	    int _whence);
	int (*fops_open)(const sos_devltr _ch, struct _sword_dir *_dirp);
	int (*fops_readdir)(const struct _sword_dir *_dir, struct _storage_fib *_fibp);
	int (*fops_closedir)(struct _sword_dir *_dir);
};

#endif  /*  _FS_VFS_H  */
