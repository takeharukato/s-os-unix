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

int
main(int argc, char *argv[]){
	int            rc;
	int                                         i;
	struct _sword_header_packet     *pkt, hdr_pkt;
	struct _fs_dir_stream                     dir;
	struct _fs_ioctx                        ioctx;
	vfs_mnt_flags mnt_flags=FS_VFS_MNT_OPT_RDONLY;
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

	rc = fs_vfs_mnt_mount_filesystem('A', FS_SWD_FSNAME,
	    (void *)(uintptr_t)mnt_flags, &ioctx);
	sos_assert( rc == 0 );

	rc = fs_vfs_path_to_vnode('A', &ioctx, "SAMPLE1.ASM", &v);
	sos_assert( rc == 0 );
	sos_assert( ( v->vn_fib.fib_attr & SOS_FATTR_ASC ) != 0 );

	rc = vfs_put_vnode(v);
	sos_assert( rc == 0 );

	rc = fs_vfs_path_to_vnode('A', &ioctx, "/", &dirv);
	sos_assert( rc == 0 );
	sos_assert( ( dirv->vn_fib.fib_attr & SOS_FATTR_DIR ) != 0 );

	rc = vfs_put_vnode(dirv);
	sos_assert( rc == 0 );

	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', &ioctx, "SAMPLE1.ASM", FS_VFS_FD_FLAG_O_WRONLY,
	    pkt, &fd, &res);
	sos_assert( rc == -1 );
	sos_assert( res == SOS_ERROR_RDONLY );

	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', &ioctx, "SAMPLE1.ASM", FS_VFS_FD_FLAG_O_RDWR,
	    pkt, &fd, &res);
	sos_assert( rc == -1 );
	sos_assert( res == SOS_ERROR_RDONLY );

	pkt->hdr_attr = SOS_FATTR_BIN;
	rc = fs_vfs_open('A', &ioctx, "SAMPLE1.ASM", FS_VFS_FD_FLAG_O_RDONLY,
	    pkt, &fd, &res);
	sos_assert( rc == -1 );
	sos_assert( res == SOS_ERROR_SYNTAX );

	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', &ioctx, "SAMPLE1.ASM", FS_VFS_FD_FLAG_O_RDONLY,
	    pkt, &fd, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_close(&ioctx, fd, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_mnt_unmount_filesystem('A', &ioctx);
	sos_assert( rc == 0 );


	free(buf1);

	return 0;
}
