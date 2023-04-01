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
#include "storage.h"

int fs_sword2unix(const BYTE *_swordname, unsigned char **_destp);
int fs_unix2sword(const unsigned char *_unixname, BYTE *_dest, size_t _size);
int fs_compare_unix_and_sword(const unsigned char *_unixname, const BYTE *_sword, size_t _len);
void fs_get_sos_header(const struct _storage_fib *_fib, void *_dest, size_t _bufsiz);
#endif  /*  _FS_SWORD_H  */
