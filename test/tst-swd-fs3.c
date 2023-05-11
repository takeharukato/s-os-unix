#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#include "disk-2d.h"
#include "sim-type.h"
#include "misc.h"
#include "fs-vfs.h"
#include "storage.h"
#include "fs-sword.h"

static ssize_t
read_file_test(struct _fs_ioctx *ioctx, int fd){
	int           rc;
	BYTE         res;
	char buf[BUFSIZ];
	size_t       cnt;
	size_t     rdcnt;

	rdcnt = 0;
	do{
		cnt = 0;
		rc = fs_vfs_read(ioctx, fd, &buf[0], BUFSIZ, &cnt, &res);
		if ( ( rc != 0 ) || ( cnt == 0 ) )
			break;
		rdcnt += cnt;
		printf("Read:%lu byte\n", cnt);
	}while( res == 0 );

	return rdcnt;
}

int
main(int argc, char *argv[]){
	int            rc;
	int                                         i;
	struct _sword_header_packet     *pkt, hdr_pkt;
	struct _fs_dir_stream                     dir;
	struct _fs_ioctx                        ioctx;
	vfs_mnt_flags                       mnt_flags;
	struct _fs_vnode                           *v;
	struct _fs_vnode                        *dirv;
	char                                    *buf1;
	int                                        fd;
	BYTE                                      res;

	storage_init();
	fs_vfs_init_vfs();

	storage_2dimg_init();
	init_sword_filesystem();

	fs_vfs_init_ioctx(&ioctx);

	if ( 2 > argc )
		return 0;

	buf1 = malloc(SOS_MAX_FILE_SIZE);
	if ( buf1 == NULL )
		exit(1);

	pkt = &hdr_pkt;
	memset(pkt, 0, sizeof(struct _sword_header_packet));

	rc = storage_mount_image('A', argv[1]);
	sos_assert( rc == 0 );

	mnt_flags = 0;
	rc = fs_vfs_mnt_mount_filesystem('A', FS_SWD_FSNAME,
	    (void *)(uintptr_t)mnt_flags, &ioctx);
	sos_assert( rc == 0 );

	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', &ioctx, "SAMPLE1.ASM", FS_VFS_FD_FLAG_O_RDONLY,
	    pkt, &fd, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = read_file_test(&ioctx, fd);
	sos_assert( rc > 0 );

	rc = fs_vfs_close(&ioctx, fd, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_mnt_unmount_filesystem('A', &ioctx);
	sos_assert( rc == 0 );

	free(buf1);

	return 0;
}
