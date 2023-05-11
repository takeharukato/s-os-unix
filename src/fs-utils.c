/* -*- mode: C; coding:utf-8 -*- */
/**********************************************************************/
/*  SWORD Emulator utilities for file system module                   */
/*                                                                    */
/*  Copyright 2023 Takeharu KATO                                      */
/*                                                                    */
/**********************************************************************/

#include <string.h>
#include <errno.h>

#include "config.h"

#include "freestanding.h"
#include "bswapmac.h"
#include "sim-type.h"
#include "misc.h"
#include "sos.h"
#include "fs-vfs.h"
#include "storage.h"
#include "fs-sword.h"

/** File type name index
 */
#define SOS_DIR_FSTRIDX_UNKNOWN (0)  /**< Unknown */
#define SOS_DIR_FSTRIDX_ASC     (1)  /**< Ascii */
#define SOS_DIR_FSTRIDX_BIN     (2)  /**< Binary */
#define SOS_DIR_FSTRIDX_BAS     (3)  /**< Basic */
#define SOS_DIR_FSTRIDX_DIR     (4)  /**< Directory */

#define SOS_FSUTILS_DIR_FMT  "%.3s%c %c:%.13s.%.3s:%04X:%04X:%04X\n"

/** File type names
 */
static const char *ftype_name_tbl[]={
	"???",
	"Asc",
	"Bin",
	"Bas",
	"Dir"
};

/** Get file type
    @param[in] attr File attribute
    @return File type string.
 */
static const char *
get_ftype(fs_attr attr){

	switch( SOS_FATTR_GET_ALL_FTYPE(attr) ){

	case SOS_FATTR_BIN:
		return ftype_name_tbl[SOS_DIR_FSTRIDX_BIN];

	case SOS_FATTR_ASC:
		return ftype_name_tbl[SOS_DIR_FSTRIDX_ASC];

	case SOS_FATTR_BAS:
		return ftype_name_tbl[SOS_DIR_FSTRIDX_BAS];

	case SOS_FATTR_DIR:
		return ftype_name_tbl[SOS_DIR_FSTRIDX_DIR];

	default:
		break;
	}

	return ftype_name_tbl[SOS_DIR_FSTRIDX_UNKNOWN];
}

/** convert from a SWORD file name to the UNIX file name
    @param[in]  swordname The file name on SWORD
    @param[in]  destp     The address of the pointer to point UNIX file name.
    @retval     0         success
    @retval     ENOMEM    out of memory
    @remark *destp should be freed by the caller after use it.
 */
int
fs_sword2unix(const BYTE *swordname, char **destp){
	int             rc;
	int              i;
	size_t   fname_len;
	size_t     ext_len;
	size_t      bufsiz;
	char          *res;

	/*
	 * Calculate the file name length
	 */
	fname_len = 0;
	for(i = SOS_FNAME_NAMELEN - 1; i >= 0; --i) {

		/*
		 * Find the last non-space character in the file name.
		 */
		if ( swordname[i] != SCR_SOS_SPC ) {

			fname_len = i + 1;
			break;
		}
	}

	/*
	 * Calculate the extention length
	 */
	ext_len = 0;
	for(i = SOS_FNAME_EXTLEN - 1; i >= 0; --i) {

		/*
		 * Find the last non-space character in the extention.
		 */
		if ( swordname[SOS_FNAME_NAMELEN + i] != SCR_SOS_SPC ) {

			ext_len = i + 1;
			break;
		}
	}
	if ( ext_len > 0 )
		bufsiz = fname_len + ext_len + 2; /* fname + '.' + ext + '\0' */
	else
		bufsiz = fname_len + 1;  /* fname + '\0' */

	/*
	 * copy file name
	 */
	res = malloc(bufsiz);
	if ( res == NULL ) {

		rc = ENOMEM;  /* Out of memory */
		goto error_out;
	}

	memcpy(res, &swordname[0], fname_len);
	res[fname_len]='\0';  /* Terminate */

	if ( ext_len > 0 ) {

		res[fname_len]='.';
		memcpy(&res[fname_len + 1], &swordname[SOS_FNAME_NAMELEN], ext_len);
	}
	res[bufsiz - 1] = '\0';  /* Terminate */

	if ( destp != NULL )
		*destp = res;

	return 0;

error_out:
	return rc;
}

/** convert from a UNIX file name to the SWORD file name
    @param[in]  unixname  The file name on UNIX
    @param[out] dest      The destination address
    @param[in]  size      The size of the buffer pointed by DEST
    @retval     0         success
 */
int
fs_unix2sword(const char *unixname, BYTE *dest, size_t size){
	int                                i;
	const char                       *sp;
	char                             *ep;
	size_t                           len;
	char         swd_name[SOS_FNAME_LEN];

	/*
	 * Skip drive letter
	 */
	sp = strchr((char *)&unixname[0],':');
	if ( sp != NULL )
		while( *sp == ':' )
			++sp;
	else
		sp = (const char *)&unixname[0];
	/* Fill spaces */
	memset(&swd_name[0], SCR_SOS_SPC, SOS_FNAME_LEN);
	/*
	 * Copy file name
	 */
	ep = strrchr((const char *)unixname,'.');
	if ( ep != NULL )
		while( *ep == '.' )
			++ep;  /* skip sequential dots */

	if ( ep == NULL ) /* UNIX name has no extention. */
		len = SOS_MIN(SOS_FNAME_NAMELEN, strlen(sp) );
	else
		len = SOS_MIN(SOS_FNAME_NAMELEN, ep - sp - 1);
	for( i = 0; len > i; ++i)
		swd_name[i] = sp[i];  /* copy file name */

	/*
	 * Copy extention
	 */
	if ( ep != NULL ) { /* UNIX name has an extention. */

		/* copy extention part */
		len = SOS_MIN(SOS_FNAME_EXTLEN, strlen(ep) );
		memcpy(&swd_name[SOS_FNAME_NAMELEN], ep, len);
	}

	/*
	 * Copy sword name to the destination
	 */
	len = SOS_MIN(size, SOS_FNAME_LEN);
	if ( dest != NULL )
		memmove(dest, &swd_name[0], len);

	return 0;
}

/** Compare UNIX file name and sword file name
    @param[in] unix  The file name on UNIX
    @param[in] sword The file name on SWORD
    @retval    0          file names are matched.
    @retval    negative   UNIX file name is lesser than SWORD
    @retval    positive   UNIX file name is grater than SWORD
 */
int
fs_compare_unix_and_sword(const char *unixname, const BYTE *sword, size_t len){
	size_t                 cmp_len;
	BYTE  conv_name[SOS_FNAME_LEN];

	cmp_len = SOS_MIN(SOS_FNAME_LEN, len);
	fs_unix2sword(unixname, &conv_name[0], SOS_FNAME_LEN);

	return memcmp(&conv_name[0], sword, cmp_len);
}

/** Get S-OS header excluding NULL terminate.
    @param[in]  fib    The file information block of the file
    @param[out] dest   The address of the buffer to store S-OS header
    @param[in]  bufsiz The size of the address of the buffer to store S-OS header
 */
void
fs_get_sos_header(const struct _storage_fib *fib, void *dest,
    size_t bufsiz){
	char   header[SOS_HEADER_BUFLEN];
	size_t                    cpysiz;

	cpysiz = SOS_MIN(bufsiz, SOS_HEADER_LEN);

	snprintf(&header[0], SOS_HEADER_BUFLEN, SOS_HEADER_PAT,
	    SOS_FATTR_VAL(fib->fib_attr),
	    SOS_Z80MEM_VAL(fib->fib_dtadr),
	    SOS_Z80MEM_VAL(fib->fib_exadr) );
	memcpy(dest, &header[0], cpysiz);
}

/** Show a directory
    @param[in] ch     The drive letter
    @param[in] ioctx  The current I/O context
    @param[in] path   The address of the buffer to store S-OS header
    @retval    0      Success
    @retval   SOS_ERROR_OFFLINE Device offline
    @retval   SOS_ERROR_IO      I/O Error
    @retval   SOS_ERROR_NOENT   Directory not found
 */
int
fs_show_dir(sos_devltr ch, struct _fs_ioctx *ioctx, const char *path){
	int                            rc;
	char fname[SOS_FNAME_NAME_BUFSIZ];
	char    ext[SOS_FNAME_EXT_BUFSIZ];
	struct _storage_fib           fib;
	struct _fs_dir_stream         dir;
	BYTE                          res;

	rc = fs_vfs_opendir(ch, ioctx, path, &dir, &res);
	if ( rc != 0 )
		return rc;

	for( ; ; ) {

		rc = fs_vfs_readdir(&dir, &fib, &res);
		if ( rc != 0 )
			break;

		memcpy(&fname[0], &fib.fib_sword_name[0], SOS_FNAME_NAMELEN);
		memcpy(&ext[0], &fib.fib_sword_name[SOS_FNAME_NAMELEN],
		    SOS_FNAME_EXTLEN);
		fname[SOS_FNAME_NAMELEN] = '\0';
		ext[SOS_FNAME_EXTLEN] = '\0';
		printf(SOS_FSUTILS_DIR_FMT,
		    get_ftype(fib.fib_attr),
		    (fib.fib_attr & SOS_FATTR_RDONLY)? ('*') : (' '),
		    fib.fib_devltr,
		    &fname[0],
		    &ext[0],
		    SOS_Z80MEM_VAL(fib.fib_dtadr),
		    SOS_Z80MEM_VAL(fib.fib_dtadr + fib.fib_size - 1),
		    SOS_Z80MEM_VAL(fib.fib_exadr));
	}

	rc = 0;

	if ( res != SOS_ERROR_NOENT )
		rc = res;

	fs_vfs_closedir(&dir, &res);  /* close dir */

	return rc;
}
