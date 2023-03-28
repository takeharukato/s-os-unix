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

int fs_sword2unix(BYTE *_swordname, unsigned char **_destp);
int fs_unix2sword(unsigned char *_unixname, BYTE *_dest, size_t _size);
int fs_compare_unix_and_sword(unsigned char *_unixname, BYTE *_sword, size_t _len);
#endif  /*  _FS_SWORD_H  */
