#include <stdio.h>
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
int fops_close_sword(struct _sword_file_descriptor *fdp, BYTE *resp);
int fops_unlink_sword(struct _sword_dir *dir, const unsigned char *path, BYTE *resp);
int fops_opendir_sword(struct _sword_dir *dir, BYTE *resp);
int fops_closedir_sword(struct _sword_dir *_dir, BYTE *_resp);

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
fs_vfs_closedir(struct _sword_dir *dirp, BYTE *resp){
	int   rc;
	BYTE res;

	if ( dirp->dir_sysflags & FS_VFS_FD_FLAG_SYS_OPENED ) {

		res = SOS_ERROR_BADF;
		goto error_out;
	}

	rc = fops_closedir_sword(dirp, &res);
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

int
main(int argc, char *argv[]){
	int   rc;
	WORD flags;
	struct _sword_file_descriptor fd, *fdp;
	struct _sword_header_packet *pkt, hdr_pkt;
	struct _sword_dir dir;
	BYTE res;

	storage_init();
	storage_2dimg_init();

	if ( 2 > argc )
		return 0;

	pkt = &hdr_pkt;
	memset(pkt, 0, sizeof(struct _sword_header_packet));

	rc = storage_mount_image('A', argv[1]);
	sos_assert( rc == 0 );

	rc = storage_set_dirps('A', SOS_DIRPS_DEFAULT);
	sos_assert( rc == 0 );
	rc = storage_set_fatpos('A', SOS_FATPOS_DEFAULT);
	sos_assert( rc == 0 );

	/*
	 * Open/Close
	 */
	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', "SAMPLE1.ASM",
	    FS_VFS_FD_FLAG_O_RDONLY, pkt, &fd, &res);
	sos_assert( res == 0 );

	rc = fs_vfs_close(&fd, &res);
	sos_assert( res == 0 );

	/* close error (double close) */
	rc = fs_vfs_close(&fd, &res);
	sos_assert( res == SOS_ERROR_NOTOPEN );

	/* Invalid file attribute */
	pkt->hdr_attr = SOS_FATTR_BIN;
	rc = fs_vfs_open('A', "SAMPLE1.ASM",
	    FS_VFS_FD_FLAG_O_RDONLY, pkt, &fd, &res);
	sos_assert( res == SOS_ERROR_NOENT );

	rc = fs_vfs_close(&fd, &res);
	sos_assert( res != 0 );

	/* file not found */
	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', "NOEXISTS.ASM",
	    FS_VFS_FD_FLAG_O_RDONLY, pkt, &fd, &res);
	sos_assert( res == SOS_ERROR_NOENT );

	rc = fs_vfs_close(&fd, &res);
	sos_assert( res != 0 );

	/* file exists */
	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', "SAMPLE1.ASM",
	    FS_VFS_FD_FLAG_O_RDONLY|FS_VFS_FD_FLAG_O_CREAT, pkt, &fd, &res);
	sos_assert( res == SOS_ERROR_SYNTAX );

	rc = fs_vfs_close(&fd, &res);
	sos_assert( res != 0 );

	/* file name exists */
	pkt->hdr_attr = SOS_FATTR_BIN;
	rc = fs_vfs_open('A', "SAMPLE1.ASM",
	    FS_VFS_FD_FLAG_O_WRONLY|FS_VFS_FD_FLAG_O_CREAT, pkt, &fd, &res);
	sos_assert( res == SOS_ERROR_EXIST );

	rc = fs_vfs_close(&fd, &res);
	sos_assert( res != 0 );

	/* offline device */
	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('B', "NOEXISTS.ASM",
	    FS_VFS_FD_FLAG_O_RDONLY, pkt, &fd, &res);
	sos_assert( res == SOS_ERROR_OFFLINE );

	rc = fs_vfs_close(&fd, &res);
	sos_assert( res != 0 );

	rc = fs_vfs_opendir('A', &dir, &res);
	sos_assert( res == 0 );

	rc = fs_vfs_closedir(&dir, &res);
	sos_assert( res == 0 );

	/*
	 * Create test
	 */
	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', "NOEXISTS.ASM",
	    FS_VFS_FD_FLAG_O_RDWR|FS_VFS_FD_FLAG_O_CREAT, pkt, &fd, &res);
	sos_assert( res == 0 );
#if 0
	rc = fs_vfs_close(&fd, &res);
	sos_assert( res != 0 );
#endif
	return 0;
}
