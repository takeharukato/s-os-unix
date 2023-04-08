/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator - virtual file system module                       */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include <string.h>
#include <errno.h>

#include "config.h"

#include "freestanding.h"

#include "sim-type.h"
#include "misc.h"
#include "compat.h"
#include "sos.h"
#include "storage.h"
#include "fs-vfs.h"

/** Initialize the file descriptor
    @param[in]  ch   The drive letter
    @param[out] fdp  The address to the file descriptor (file handler) to initialize
 */
void
fs_vfs_init_fd(sos_devltr ch, struct _sword_file_descriptor *fdp){
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	memset(fdp, 0x0, sizeof(struct _sword_file_descriptor));   /* just in case */

	storage_init_fib(&fdp->fd_fib);       /* Initialize the file information block */
	storage_init_position(&fdp->fd_pos);  /* Initialize position */

	fib = &fdp->fd_fib;
	pos = &fdp->fd_pos;

	fib->fib_devltr = ch;  /* Device letter */

	storage_get_dirps(ch, &pos->dp_dirps);    /* FIXME: Set #DIRPS of the device */
	storage_get_fatpos(ch, &pos->dp_fatpos);  /* FIXME: Set #FATPOS of the device */

	/*
	 * Clear flags
	 */
	fdp->fd_flags = 0;
	fdp->fd_sysflags = 0;

	fdp->fd_private=NULL;  /* Initialize private information */
}

/** Initialize the directory stream
    @param[in]  ch   The drive letter
    @param[out] dir  The address to the directory stream to initialize
 */
void
fs_vfs_init_dir_stream(sos_devltr ch, struct _sword_dir *dir){
	struct _storage_fib      *fib;
	struct _storage_disk_pos *pos;

	storage_init_fib(&dir->dir_fib);      /* Initialize the file information block */
	storage_init_position(&dir->dir_pos); /* Initialize position */

	fib = &dir->dir_fib;
	pos = &dir->dir_pos;

	fib->fib_devltr = ch;  /* Device letter */

	storage_get_dirps(ch, &pos->dp_dirps);    /* FIXME: DIRPS  */
	storage_get_fatpos(ch, &pos->dp_fatpos);  /* FIXME: FATPOS */

	/*
	 * Clear flags
	 */
	dir->dir_sysflags = 0;
	dir->dir_private = NULL;
}
