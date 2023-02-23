/*
   SWORD Emurator  Disk I/O module
   $Id: dio.c,v 1.6 1997/03/31 17:48:00 tate Exp tate $

   tate@spa.is.uec.ac.jp
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include "simz80.h"
#include "dio.h"
#include "sos.h"

#define	DIO_IMAGEPAT	"sos%d.dsk"		/* default disk image file */

/* single fork header */
#define	DIO_HEADERPAT	"_SOS %02x %04x %04x\n"
#define	DIO_HEADERLEN	(18)

#define DIO_MODE_ASC	(4)
#define DIO_MODE_DEF	DIO_MODE_ASC		/* default attribute */

#if DIO_MODE_DEF == DIO_MODE_ASC		/* default is ascii mode */
# define DIO_MODE_DEF_ASC	(1)
#else
# define DIO_MODE_DEF_ASC	(0)
#endif

#define	DIO_RECLEN	(256)		/* length of a record */

static FILE	*openfp = NULL;		/* for dio_[wr]open */
static int	asciimode =0;		/* now in ascii convert mode */

static DIR	*dirfp = NULL;		/* for dio_dopen */
static int	dircurrent = -1;	/* current dirno */

char	*dio_disk[SOS_MAXIMAGEDRIVES];
static FILE	*imagefp[SOS_MAXIMAGEDRIVES];	/* for image file */


/*
   file name conversion from sword format to unix format

   return a pointer to unix filename (in static variable)
*/
char *
dio_stou(char *sosname){
    int	ni;	/* name index */
    int l;	/* length counter */
    int ei;	/* end index on name */
    int nei;	/* end of name part on name */
    static char	name[SOS_FNAMELEN+2];
    char	*p;

    for (l=ei=0; l<SOS_FNAMENAMELEN; l++){
	if ((name[l] = *sosname++) != ' '){
	    ei = l;
	}
    }
    nei = ++ei;	/* save end mark (terminate at this pointif no extension) */
    name[ei] = '.';
    for (l=0, ni=ei+1; l<SOS_FNAMEEXTLEN; l++){
	if ((name[ni++] = *sosname++) != ' '){
	    ei = ni;
	}
    }
    if (ei != nei){
	/* has extension */
	name[ei] = '\0';
    } else {
	/* no extension */
	name[nei] = '\0';
    }
    return(name);
}

/*
   file name conversion from unix format to sword format

   return a pointer to unix filename (in static variable)
*/
char *
dio_utos(char *unixname){
    static char	sosname[SOS_FNAMELEN+1];
    int	i;
    char	c;

    for (i=0; i<SOS_FNAMENAMELEN; i++){
	if ((c = *unixname) == '\0' || c == '.')
	    break;
	sosname[i] = c;
	unixname++;
    }
    for (; i<SOS_FNAMENAMELEN; i++)
	sosname[i] = ' ';		/* space padding */

    /* skip extension mark */
    if (*unixname == '.')
	unixname++;

    for (i=SOS_FNAMENAMELEN; i< SOS_FNAMELEN; i++){
	if (*unixname != '\0')
	    sosname[i] = *unixname++;
	else
	    sosname[i] = ' ';
    }
    sosname[i] = '\0';		/* paranoia */
    return(sosname);
}

/*
   write open

   write SWORD header

   return 0 if success
*/
int
dio_wopen(char *sosname, int attr, int dtadr, int size, int exadr){
    char	buf[DIO_HEADERLEN+1];
    char	*name;

    if (openfp != NULL)
	fclose(openfp);

    name = dio_stou(sosname);	/* file name conversion */

    if ((openfp = fopen(name, "wb")) == NULL){
	return(1);
    }

    /* store SWORD header */
    snprintf(buf, DIO_HEADERLEN+1, DIO_HEADERPAT ,attr, dtadr, exadr);

    if (fwrite(buf, sizeof(char), DIO_HEADERLEN, openfp) < DIO_HEADERLEN){
	fclose(openfp);
	unlink(name);
	return(1);
    }

    if (attr == DIO_MODE_ASC){
	asciimode = 1;
    } else {
	asciimode = 0;
    }

    return(0);
}

/*
   read open

   if (conv), convert filename from SWORD format to UNIX format
*/
int
dio_ropen(char *sosname, int *attr, int *dtadr, int *size,
	  int *exadr, int conv){
    char	buf[DIO_HEADERLEN + 1];
    int		fattr, fdtadr, fexadr, fsize;
    char	*name;
    size_t      rc;

    if (openfp != NULL)
	fclose(openfp);

    if (conv)
	name = dio_stou(sosname);	/* file name conversion */
    else
	name = sosname;

    if ((openfp = fopen(name, "rb")) == NULL){
	return(8);
    }

    /* check SWORD header */
    *buf = '\0';	/* paranoia */
    rc = fread(buf, sizeof(char), DIO_HEADERLEN, openfp);
    buf[DIO_HEADERLEN] = '\0';	/* paranoia */
    if (sscanf(buf, DIO_HEADERPAT, &fattr, &fdtadr, &fexadr) == 3){
	/* this is single fork file with magic */
	if (fattr == DIO_MODE_ASC){
	    asciimode = 1;
	} else {
	    asciimode = 0;
	}
	fseek(openfp, 0L, SEEK_END);
	if ((fsize = (unsigned int)ftell(openfp) - DIO_HEADERLEN + asciimode)
	    > 0xffff){
	    fsize = 0xffff;		/* truncate */
	}
	fseek(openfp, (long) DIO_HEADERLEN, SEEK_SET);
	*attr = fattr;
	*dtadr = fdtadr;
	*exadr = fexadr;
	*size = fsize;
    } else {
	/* this is plain file */
	fseek(openfp, 0L, SEEK_END);
	if ((fsize = (unsigned int)ftell(openfp) + DIO_MODE_DEF_ASC) > 0xffff){
	    fsize = 0xffff;		/* truncate */
	}
	rewind(openfp);
	*attr = DIO_MODE_DEF;		/* default attr */
	asciimode = DIO_MODE_DEF_ASC;
	*dtadr = 0;
	*exadr = 0;
	*size = fsize;
    }

    return(0);
}

/*
   directory read with attribute

   return 0 if success
*/
int
dio_dopen(char *namebuf, int *attr, int *dtadr, int *size, int *exadr, int dirno){
    struct dirent	*dp;
    int	i;
    char	*sosname;

    if (dirno == 0){		/* start from virgin */
	if (dirfp != NULL){
	    (void) closedir(dirfp);
	}
	if ((dirfp = opendir(".")) == NULL){
	    return(1);
	}
	dircurrent = 0;
    }

    if (dirno != dircurrent){
	/* Gee! why don't you read simple? */
	/* seek to requested point */
	for (i=0; i<dirno; i++){
	    if (readdir(dirfp) == NULL){
		/* end of entries */
		closedir(dirfp);
		dirfp = NULL;
		return(8);
	    }
	}
    }

    do {
	if ((dp = readdir(dirfp)) == NULL){
	    /* end of entries */
	    closedir(dirfp);
	    dirfp = NULL;
	    return(8);
	}
        dircurrent = dirno + 1;
    } while((strcmp(dp->d_name,".") == 0) || (strcmp(dp->d_name,"..") == 0));
    sosname = dio_utos(dp->d_name);
    strncpy(namebuf, sosname, SOS_FNAMELEN);

    /* read SWORD header information */
    if (dio_ropen(dp->d_name, attr, dtadr, size, exadr, 0) == 0){
	fclose(openfp);
	openfp = NULL;
    } else {
	/* header read error, fake information */
	*attr = *dtadr = *size = *exadr = 0;
    }
    return(0);		/* ignore all errors during read header */
}


/*
   write from buffer

   return 0 if success
*/
int
dio_wdd(unsigned char *buf, int len){
    unsigned char	*p, c;

    if (openfp == NULL)
	return(12);		/* file not open */
    if (asciimode)
	len--;			/* remove last '\0' datum */

    if (!asciimode){
	/* binary mode */
	if (fwrite(buf, sizeof(unsigned char), (size_t)len, openfp) < len){
	    fclose(openfp);
	    openfp = NULL;
	    return(1);
	}
    } else {
	/* ascii mode */
	p = buf;
	while(len > 0){
	    if ((c = *p) == '\r')
		c = '\n';		/* convert CR->LF */
	    fputc(c, openfp);
	    p++;
	    len--;
	}
	/* check last datum, which may not '\0' */
	if (*p != '\0')
	    fputc(*p, openfp);
    }

    fclose(openfp);
    openfp = NULL;

    return(0);
}

/*
   read info buffer

   return 0 if success
*/
int
dio_rdd(unsigned char *buf, int len){
    unsigned char	*p;

    if (openfp == NULL)
	return(12);		/* file not open */
    if (asciimode)
	len--;			/* file is not contain last '\0' */

    if (fread(buf, sizeof(unsigned char), (size_t)len, openfp) < len){
	fclose(openfp);
	openfp = NULL;
	return(1);
    }

    if (asciimode){
	p = buf;
	while(len > 0){
	    if (*p == '\n')		/* convert LF->CR */
		*p = '\r';
	    p++;
	    len--;
	}
	*p = '\0';		/* add last '\0' */
    }

    fclose(openfp);
    openfp = NULL;
    return(0);
}


/*
   raw disk I/O
*/

/*
   open disk image file
*/
FILE *
dio_diopen(int diskno){
    char	name[sizeof(DIO_IMAGEPAT)+1];

    if (imagefp[diskno] == NULL) {

	    /* not opended this drive */
	    if (dio_disk[diskno] == NULL) {

		    /* image file name is not defined, use default */
		    snprintf(name, sizeof(DIO_IMAGEPAT)+1, DIO_IMAGEPAT, diskno);

		    dio_disk[diskno] = strdup(name);
		    if ( dio_disk[diskno] == NULL )
			    return NULL;
	    }

	    imagefp[diskno] = fopen(dio_disk[diskno], "rb+");
	    return imagefp[diskno];
    }

    return imagefp[diskno];
}

/*
   make sure close the disk image file
*/
void
dio_diclose(int diskno){
    if (imagefp[diskno] != NULL){
	fclose(imagefp[diskno]);
	imagefp[diskno] = NULL;
    }
}

/*
   read from disk image file

   start from "recno" record, "numrec" records
   1 record = 256 byte
*/
int dio_dread(unsigned char *buf, int diskno, int recno, int numrec){
    FILE *fp;
    size_t	len;

    if ((fp = dio_diopen(diskno)) == NULL)
	return(2);		/* device offline */
    (void) fseek(fp, (long)recno * DIO_RECLEN, SEEK_SET);
    len = (size_t) numrec * DIO_RECLEN;
    if (fread(buf, sizeof(unsigned char), len, fp) < len){
	dio_diclose(diskno);
	return(1);		/* device I/O error */
    }
    return(0);
}

int dio_dwrite(unsigned char *buf, int diskno, int recno, int numrec){
    FILE *fp;
    size_t	len;

    if ((fp = dio_diopen(diskno)) == NULL)
	return(2);		/* device offline */
    (void) fseek(fp, (long)recno * DIO_RECLEN, SEEK_SET);
    len = (size_t) numrec * DIO_RECLEN;
    if (fwrite(buf, sizeof(unsigned char), len, fp) < len){
	dio_diclose(diskno);
	return(1);		/* device I/O error */
    }
    return(0);
}
