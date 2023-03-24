/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator 2D disk image definitions                          */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_DISK_2D_H)
#define _DISK_2D_H

#include "freestanding.h"
#include "sim-type.h"
#include "sos.h"
#include "storage.h"

/*
 * Macros
 */
#define DISK_2D_IMAGES_NR    (4)  /**< disk devices */

/*
 * Data structure
 */

/** Disk image
 */
struct _disk2d_image{
	int                     fd;  /**< file decriptor for the disk image file */
	unsigned char       *fname;  /**< file name of the disk image file */
};

/** Private information
 */
struct _disk2d_private{
	struct _disk2d_image images[DISK_2D_IMAGES_NR];
};

/*
 * Prototypes
 */
void storage_2dimg_init(void);
#endif  /*  _DISK_2D_H  */
