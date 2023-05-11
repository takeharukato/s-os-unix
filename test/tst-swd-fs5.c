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
	struct _fs_ioctx                        ioctx;
	vfs_mnt_flags                       mnt_flags;
	size_t                                    cnt;
	fs_attr                                  attr;
	fs_attr                              new_attr;
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

	/*
	 * write ascii file
	 */
	pkt->hdr_attr = SOS_FATTR_ASC;
	rc = fs_vfs_open('A', &ioctx, "MAX-SIZ.TXT",
	    FS_VFS_FD_FLAG_O_RDWR|FS_VFS_FD_FLAG_O_CREAT, pkt, &fd, &res);
	sos_assert( res == 0 );

	for(i = 0; SOS_MAX_FILE_SIZE / SOS_CLUSTER_SIZE > i; ++i){

		memset((char*)&buf1[0] + (SOS_CLUSTER_SIZE * i), '0'+ i,
		    SOS_CLUSTER_SIZE);
	}

	/* Truncate 0(No change) */
	rc = fs_vfs_truncate(&ioctx, fd, 0, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate MAX_FILE_SIZE */
	rc = fs_vfs_truncate(&ioctx, fd, SOS_MAX_FILE_SIZE, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 0(No change) */
	rc = fs_vfs_truncate(&ioctx, fd, 0, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 2 block  */
	rc = fs_vfs_truncate(&ioctx, fd, SOS_CLUSTER_SIZE, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 2.5 block without allocating blocks by shirnking  */
	rc = fs_vfs_truncate(&ioctx, fd, SOS_CLUSTER_SIZE + SOS_CLUSTER_SIZE/2, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/* Truncate 2 block without releasing blocks by shirnking */
	rc = fs_vfs_truncate(&ioctx, fd, SOS_CLUSTER_SIZE, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_close(&ioctx, fd, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	/*
	 * stat
	 */
	rc = fs_vfs_get_attr('A', &ioctx, "MAX-SIZ.TXT", &attr, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );
	sos_assert( SOS_FATTR_ASC & attr );

	rc = fs_vfs_set_attr('A', &ioctx, "MAX-SIZ.TXT", SOS_FATTR_RDONLY, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_get_attr('A', &ioctx, "MAX-SIZ.TXT", &new_attr, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );
	sos_assert( SOS_FATTR_ASC & new_attr );
	sos_assert( SOS_FATTR_RDONLY & new_attr );

	/*
	 * Rename
	 */
	rc = fs_vfs_rename('A', &ioctx, "MAX-SIZ.TXT", "MAX-SIZ2.TXT", &res);
	sos_assert( rc == -1 );
	sos_assert( res == SOS_ERROR_RDONLY );

	rc = fs_vfs_set_attr('A', &ioctx, "MAX-SIZ.TXT", 0, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_get_attr('A', &ioctx, "MAX-SIZ.TXT", &new_attr, &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );
	sos_assert( SOS_FATTR_ASC & new_attr );
	sos_assert( !(SOS_FATTR_RDONLY & new_attr ) );

	rc = fs_vfs_rename('A', &ioctx, "MAX-SIZ.TXT", "MAX-SIZ2.TXT", &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_unlink('A', &ioctx, "MAX-SIZ2.TXT", &res);
	sos_assert( rc == 0 );
	sos_assert( res == 0 );

	rc = fs_vfs_mnt_unmount_filesystem('A', &ioctx);
	sos_assert( rc == 0 );

	free(buf1);

	return 0;
}
