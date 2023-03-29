/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator SWORD file system module header                    */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#if !defined(_FS_SWORD_H)
#define _FS_SWORD_H

#include "sim-type.h"

/** dirent structure for fs_opendir
 */
struct _sword_dirent{
	sos_devltr                   ch;  /**< Device letter                      */
	BYTE                       pad1;  /**< padding                            */
	BYTE                   dp_dirno;  /**< Current #DIRNO                     */
	BYTE                  dp_retpoi;  /**< Current RETPOI                     */
	WORD                     dp_pos;  /**< File or device position            */
};


int fs_sword2unix(BYTE *_swordname, unsigned char **_destp);
int fs_unix2sword(unsigned char *_unixname, BYTE *_dest, size_t _size);
int fs_compare_unix_and_sword(unsigned char *_unixname, BYTE *_sword, size_t _len);
#endif  /*  _FS_SWORD_H  */
