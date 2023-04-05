#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "storage.h"
#include "disk-2d.h"
#include "sim-type.h"
#include "misc.h"
#include "fs-vfs.h"
#include "fs-sword.h"

int fops_open_sword(sos_devltr ch, const char *fname, WORD flags,
    const struct _sword_header_packet *pkt, struct _storage_fib *fibp,
    void **privatep, BYTE *resp);
int fops_read_sword(struct _sword_file_descriptor *fdp, void *dest, size_t count,
    size_t *rdsizp, BYTE *resp);
int fops_write_sword(struct _sword_file_descriptor *fdp, const void *src,
    size_t count, size_t *wrsizp, BYTE *resp);
int fops_close_sword(struct _sword_file_descriptor *fdp, BYTE *resp);
int fops_unlink_sword(struct _sword_dir *dir, const unsigned char *path, BYTE *resp);
int fops_opendir_sword(struct _sword_dir *dir, BYTE *resp);
int fops_closedir_sword(struct _sword_dir *_dir, BYTE *_resp);
int fops_readdir_sword(struct _sword_dir *dir, struct _storage_fib *fib, BYTE *resp);
int fops_unlink_sword(struct _sword_dir *dir, const unsigned char *path, BYTE *resp);
int fops_truncate_sword(struct _sword_file_descriptor *fdp, fs_off_t offset, BYTE *resp);

const unsigned char *ftype_name_tbl[]={
	"???",
	"Asc",
	"Bin"};

#define SOS_DIR_FSTRIDX_UNKNOWN (0)
#define SOS_DIR_FSTRIDX_ASC     (1)
#define SOS_DIR_FSTRIDX_BIN     (2)

static int
get_ftype_idx(BYTE attr){

	switch( SOS_FATTR_GET_FTYPE(attr) ){

	case SOS_FATTR_BIN:
		return SOS_DIR_FSTRIDX_BIN;

	case SOS_FATTR_ASC:
		return SOS_DIR_FSTRIDX_ASC;

	default:
		break;
	}

	return SOS_DIR_FSTRIDX_UNKNOWN;
}

void
print_unix_filename(BYTE *name){
	int rc;
	unsigned char *unixname;

	rc = fs_sword2unix(&name[0], &unixname);
	printf("UNIX:%s\n", unixname);
	free(unixname);
}

void
print_sword_filename(BYTE *name){
	int i;

	printf("SWD :");
	for(i=0;SOS_FNAME_LEN>i;++i)
		putchar(name[i]);

	puts("");

	fflush(stdout);
}

void
fd_init(sos_devltr ch, struct _sword_file_descriptor *fdp){
	BYTE dent[SOS_DENTRY_SIZE];
	struct _storage_disk_pos *pos;

	pos = &fdp->fd_pos;

	memset(&dent[0],0x0,SOS_DENTRY_SIZE);

	memset(fdp, 0x0, sizeof(struct _sword_file_descriptor));

	fdp->fd_flags = 0;
	fdp->fd_sysflags = 0;

	storage_init_position(&fdp->fd_pos);

	pos->dp_devltr = ch;

	storage_get_dirps(ch, &pos->dp_dirps);

	storage_get_fatpos(ch, &pos->dp_fatpos);

	STORAGE_FILL_FIB(&fdp->fd_fib, ch, 0, &dent[0]);
	fdp->fd_private=NULL;
}

static int
init_dir_stream(sos_devltr ch, struct _sword_dir *dir){
	struct _storage_disk_pos *pos;

	pos = &dir->dir_pos;

	storage_init_position(pos);

	pos->dp_devltr = ch;
	storage_get_dirps(ch, &pos->dp_dirps);
	storage_get_fatpos(ch, &pos->dp_fatpos);
	dir->dir_sysflags = 0;
	dir->dir_private = NULL;
}

static int
fs_vfs_opendir(sos_devltr ch, struct _sword_dir *dirp,
    BYTE *resp){
	int                rc;
	struct _sword_dir dir;
	BYTE              res;

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		res = SOS_ERROR_OFFLINE;
		goto error_out;
	}
	if ( rc != 0 ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	init_dir_stream(ch, &dir);

	rc = fops_opendir_sword(&dir, &res);
	if ( rc != 0 )
		goto error_out;

	memcpy(dirp, &dir, sizeof(struct _sword_dir));

	if ( resp != NULL )
		*resp = 0;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;

	return -1;
}

static int
fs_vfs_readdir(struct _sword_dir *dir, struct _storage_fib *fibp, BYTE *resp){
	int   rc;
	BYTE res;
	struct _storage_fib fib;

	if ( dir->dir_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	rc = fops_readdir_sword(dir, &fib, &res);
	if ( rc != 0 )
		goto error_out;

	memcpy(fibp, &fib, sizeof(struct _storage_fib));

	if ( resp != NULL )
		*resp = 0;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;
	return -1;
}

static int
fs_vfs_closedir(struct _sword_dir *dir, BYTE *resp){
	int   rc;
	BYTE res;

	if ( dir->dir_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	rc = fops_closedir_sword(dir, &res);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = 0;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;
	return -1;
}

static int
fs_vfs_open(sos_devltr ch, const char *filepath, WORD flags,
    const struct _sword_header_packet *pkt, struct _sword_file_descriptor *fdp,
    BYTE *resp){
	int                                   rc;
	struct _sword_file_descriptor fd, *fdref;
	BYTE                                 res;

	rc = storage_check_status(ch);
	if ( rc == ENXIO ) {

		res = SOS_ERROR_OFFLINE;
		goto error_out;
	}
	if ( rc != 0 ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	fd_init(ch, &fd);  /* Initialize file descriptor */
	fdref = &fd;

	rc = fops_open_sword(ch, filepath, flags, pkt,
	    &fdref->fd_fib, &fdref->fd_private, &res);
	if ( res != 0 )
		goto error_out;

	memcpy(fdp, fdref, sizeof(struct _sword_file_descriptor));

	fdp->fd_flags = flags; /* remember file open flags */
	fdp->fd_sysflags |= FS_VFS_FD_FLAG_SYS_OPENED;  /* set file opened */

	if ( resp != NULL )
		*resp = res;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;
	return -1;
}

static int
fs_vfs_close(struct _sword_file_descriptor *fdp, BYTE *resp){
	int   rc;
	BYTE res;

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	rc = fops_close_sword(fdp, &res);
	if ( res != 0 )
		goto error_out;

	fd_init(0, fdp);  /* Clear file descriptor status */

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;

	return -1;
}

static int
fs_vfs_truncate(struct _sword_file_descriptor *fdp, fs_off_t offset, BYTE *resp){
	int   rc;
	BYTE res;

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	rc = fops_truncate_sword(fdp, offset, &res);
	if ( res != 0 )
		goto error_out;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;

	return -1;
}

static int
fs_vfs_unlink(struct _sword_dir *dir, const unsigned char *path, BYTE *resp){
	int   rc;
	BYTE res;

	if ( dir->dir_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	rc = fops_unlink_sword(dir, path, &res);
	if ( rc != 0 )
		goto error_out;

	if ( resp != NULL )
		*resp = 0;

	return 0;

error_out:
	if ( resp != NULL )
		*resp = res;
	return -1;
}

static int
fs_vfs_read(struct _sword_file_descriptor *fdp, void *buf, size_t count,
    size_t *rwcntp, BYTE *resp){
	int                        rc;
	size_t                  rdsiz;
	BYTE                      res;
	struct _storage_disk_pos *pos;

	pos = &fdp->fd_pos;

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	rdsiz = 0;  /* Init read size */
	rc = fops_read_sword(fdp, buf, count, &rdsiz, &res);
	if ( rc != 0 )
		goto error_out;

	pos->dp_pos += rdsiz;  /* update position */

	res = 0;

error_out:
	if ( rwcntp != NULL )
		*rwcntp = rdsiz;

	if ( resp != NULL )
		*resp = res;

	return rc;
}

static int
fs_vfs_write(struct _sword_file_descriptor *fdp, const void *buf, size_t count,
    size_t *rwcntp, BYTE *resp){
	int                        rc;
	size_t                  wrsiz;
	BYTE                      res;
	struct _storage_disk_pos *pos;

	pos = &fdp->fd_pos;

	if ( !( fdp->fd_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) ) {

		res = SOS_ERROR_NOTOPEN;
		goto error_out;
	}

	if ( !( fdp->fd_flags & FS_VFS_FD_FLAG_MAY_WRITE ) ) {

		res = SOS_ERROR_NOTOPEN;  /* The file is not opend to write. */
		goto error_out;
	}

	wrsiz = 0;  /* Init written size */

	rc = fops_write_sword(fdp, buf, count, &wrsiz, &res);
	if ( rc != 0 )
		goto error_out;

	pos->dp_pos += wrsiz;  /* update position */

	res = 0;

error_out:
	if ( rwcntp != NULL )
		*rwcntp = wrsiz;

	if ( resp != NULL )
		*resp = res;

	return rc;
}

static int
show_dir(sos_devltr ch){
	int   rc;
	int   idx;
	unsigned char fname[SOS_FNAME_NAME_BUFSIZ];
	unsigned char ext[SOS_FNAME_EXT_BUFSIZ];
	struct _storage_fib fib;
	struct _sword_dir dir;
	BYTE res;

	rc = fs_vfs_opendir('A', &dir, &res);
	if ( rc != 0 )
		return res;

	do{
		rc = fs_vfs_readdir(&dir, &fib, &res);
		if ( rc != 0 )
			break;

		memcpy(&fname[0], &fib.fib_sword_name[0], SOS_FNAME_NAMELEN);
		memcpy(&ext[0], &fib.fib_sword_name[SOS_FNAME_NAMELEN],
		    SOS_FNAME_EXTLEN);
		fname[SOS_FNAME_NAMELEN] = '\0';
		ext[SOS_FNAME_EXTLEN] = '\0';
		idx = get_ftype_idx(fib.fib_attr);
			printf("%.3s %c:%.13s.%.3s:%04X:%04X:%04X\n",
			    ftype_name_tbl[idx],
			    fib.fib_devltr,
			    &fname[0],
			    &ext[0],
			    SOS_Z80MEM_VAL(fib.fib_dtadr),
			    SOS_Z80MEM_VAL(fib.fib_dtadr + fib.fib_size - 1),
			    SOS_Z80MEM_VAL(fib.fib_exadr));
	}while( rc == 0 );
	if ( res != SOS_ERROR_NOENT ) {

		fs_vfs_closedir(&dir, &res);
		return res;
	}

	fs_vfs_closedir(&dir, &res);

	return 0;
}
static int
read_file_test(struct _sword_file_descriptor *fdp){
	int           rc;
	BYTE         res;
	char buf[BUFSIZ];
	size_t       cnt;

	do{

		rc = fs_vfs_read(fdp, &buf[0], BUFSIZ, &cnt, &res);
		if ( rc != 0 )
			break;
		printf("Read:%lu byte\n", cnt);
	}while( res == 0 );

}
int
main(int argc, char *argv[]){
	int            rc;
	int             i;
	fs_fd_flags flags;
	struct _sword_file_descriptor fd;
	struct _sword_header_packet *pkt, hdr_pkt;
	struct _sword_dir dir;
	char *buf1;
	BYTE res;

	storage_init();
	storage_2dimg_init();

	if ( 2 > argc )
		return 0;

	buf1 = malloc(SOS_MAX_FILE_SIZE);
	if ( buf1 == NULL )
		exit(1);

	pkt = &hdr_pkt;
	memset(pkt, 0, sizeof(struct _sword_header_packet));

	rc = storage_mount_image('A', argv[1]);
	sos_assert( rc == 0 );

	rc = storage_set_dirps('A', SOS_DIRPS_DEFAULT);
	sos_assert( rc == 0 );
	rc = storage_set_fatpos('A', SOS_FATPOS_DEFAULT);
	sos_assert( rc == 0 );

	show_dir('A');
	/*
	 * write ascii file
	 */
	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', "MAX-SIZ.TXT",
	    FS_VFS_FD_FLAG_O_RDWR|FS_VFS_FD_FLAG_O_CREAT, pkt, &fd, &res);
	sos_assert( res == 0 );

	for(i = 0; SOS_MAX_FILE_SIZE / SOS_CLUSTER_SIZE > i; ++i){

		memset((char*)&buf1[0] + (SOS_CLUSTER_SIZE * i), '0'+ i,
		    SOS_CLUSTER_SIZE);
	}

	/* Truncate 0(No change) */
	rc = fs_vfs_truncate(&fd, 0, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate MAX_FILE_SIZE */
	rc = fs_vfs_truncate(&fd, SOS_MAX_FILE_SIZE, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 0 */
	rc = fs_vfs_truncate(&fd, 0, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 2 block  */
	rc = fs_vfs_truncate(&fd, SOS_CLUSTER_SIZE, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 2.5 block without allocating blocks by shirnking  */
	rc = fs_vfs_truncate(&fd, SOS_CLUSTER_SIZE + SOS_CLUSTER_SIZE/2, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 2 block without releasing blocks by shirnking */
	rc = fs_vfs_truncate(&fd, SOS_CLUSTER_SIZE, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_close(&fd, &res);
	sos_assert( res == 0 );

	printf("After Truncate\n");
	show_dir('A');


	/*
	 * remove file
	 */

	rc = fs_vfs_opendir('A', &dir, &res);
	sos_assert( res == 0 );

	rc = fs_vfs_unlink(&dir, "MAX-SIZ.TXT", &res);
	sos_assert( res == 0 );

	rc = fs_vfs_closedir(&dir, &res);
	sos_assert( res == 0 );

	printf("After unlink\n");
	show_dir('A');

	free(buf1);

	return 0;
}
