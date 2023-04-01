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
#include "storage.h"

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
fs_unix2sword(const unsigned char *unixname, BYTE *dest, size_t size){
	int                                i;
	const char                       *sp;
	char                             *ep;
	size_t                           len;
	char         swd_name[SOS_FNAME_LEN];

	/*
	 * Skip drive letter
	 */
	sp = strchr(&unixname[0],':');
	if ( sp != NULL )
		while( *sp == ':' )
			++sp;
	else
		sp = &unixname[0];
	/* Fill spaces */
	memset(&swd_name[0], SCR_SOS_SPC, SOS_FNAME_LEN);
	/*
	 * Copy file name
	 */
	ep = strrchr(unixname,'.');
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
fs_compare_unix_and_sword(const unsigned char *unixname, const BYTE *sword, size_t len){
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
	unsigned char header[SOS_HEADER_BUFLEN];
	size_t                           cpysiz;

	cpysiz = SOS_MIN(bufsiz, SOS_HEADER_LEN);

	snprintf(&header[0], SOS_HEADER_BUFLEN, SOS_HEADER_PAT,
	    SOS_FATTR_VAL(fib->fib_attr),
	    SOS_Z80MEM_VAL(fib->fib_dtadr),
	    SOS_Z80MEM_VAL(fib->fib_exadr) );
	memcpy(dest, &header[0], cpysiz);
}
