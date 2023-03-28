#include <stdio.h>
#include <errno.h>

#include "storage.h"
#include "disk-2d.h"
#include "sim-type.h"
#include "misc.h"

int
main(int argc, char *argv[]){
	int                         rc;
	int                          i;
	BYTE              temp[BUFSIZ];
	BYTE      rec[SOS_RECORD_SIZE];
	WORD                       len;
	struct _storage_disk_image img;
	struct _storage_fib        fib;

	storage_init();
	storage_2dimg_init();

	if ( 1 > argc )
		return 0;

	rc = storage_mount_image('A', argv[1]);
	sos_assert( rc == 0 );
	rc = storage_unmount_image('A');
	sos_assert( rc == 0 );

	rc = storage_mount_image('A', argv[1]);
	sos_assert( rc == 0 );

	rc = storage_mount_image('A', argv[1]);
	sos_assert( rc == EBUSY );

	rc = storage_get_image_info('A', &img);
	sos_assert( rc == 0 );

	rc = storage_fib_read('A', 0, &fib);
	sos_assert( rc == ENOENT );

	rc = storage_fib_write('A', 0, (const struct _storage_fib *)&fib);
	sos_assert( rc == ENOENT );

	rc = storage_seq_read('A', &temp[0], BUFSIZ);
	sos_assert( rc == ENOENT );

	rc = storage_seq_write('A', &temp[0], BUFSIZ);
	sos_assert( rc == ENOENT );

	for(i = 0; 640 > i; ++i) {

		rc = storage_record_read('A', &rec[0], i, 1, &len);
		if ( i == SOS_DIRPS_DEFAULT )
			continue;

		if ( rc != 0 )
			break;

		sos_assert( len == 1 );
	}
	return 0;
}
