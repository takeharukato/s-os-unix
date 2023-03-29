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

/** File descriptor
 */
struct _sword_file_descriptor{
	int                    fd_flags;  /**< Open flags */
	struct _storage_disk_pos fd_pos;  /**< Position Information */
	struct _storage_fib      fd_fib;  /**< File Information Block */
	void                *fd_private;  /**< Private Information */
};

/** dirent structure for fs_opendir
 */
struct _sword_dirent{
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
       int (*fops_stat)(struct _sword_file_descriptor *_fdp, struct _storage_fib *_fibp);
       int (*fops_seek)(struct _sword_file_descriptor *_fdp, fs_off_t _offset,
	   int _whence);
       int (*fops_opendir)(const sos_devltr _ch, const struct _sword_dirent *_dent);
       int (*fops_closedir)(struct _sword_dirent *_dent);
};

#endif  /*  _FS_VFS_H  */
