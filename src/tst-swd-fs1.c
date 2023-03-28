#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "storage.h"
#include "disk-2d.h"
#include "sim-type.h"
#include "misc.h"
#include "fs-sword.h"

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
int
main(int argc, char *argv[]){
	int                         rc;
	int                          i;
	BYTE              temp[BUFSIZ];
	BYTE      rec[SOS_RECORD_SIZE];
	BYTE     rec2[SOS_RECORD_SIZE];
	BYTE swdname[SOS_FNAME_BUFSIZ];
	WORD                       len;
	unsigned char        *unixname;
	struct _storage_disk_image img;
	struct _storage_fib        fib;

	storage_init();
	storage_2dimg_init();

	if ( 2 > argc )
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

	rc = fs_unix2sword("e-mate.obj", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_unix2sword("b:zeda.obj", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_unix2sword("q:readme", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_unix2sword("q:.git", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_compare_unix_and_sword(".git", &swdname[0], SOS_FNAME_BUFSIZ);
	sos_assert( rc == 0 );

	rc = fs_unix2sword("", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_unix2sword("q:", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_unix2sword("q:0123456789abcdef", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_compare_unix_and_sword("abcdef", &swdname[0], SOS_FNAME_BUFSIZ);
	printf("abcdef vs 0123456789abcdef=%d\n", rc);
	rc = fs_compare_unix_and_sword("q:0123456789abcdef",
	    &swdname[0], SOS_FNAME_BUFSIZ);
	printf("q:0123456789abcdef vs 0123456789abcdef=%d\n", rc);
	rc = fs_compare_unix_and_sword("q:.0123456789abcdef",
	    &swdname[0], SOS_FNAME_BUFSIZ);
	printf("q:.0123456789abcdef vs 0123456789abcdef=%d\n", rc);

	rc = fs_unix2sword("q:0123456789abcdef.", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = fs_unix2sword("q:01 3 5 7 9 b.a z", &swdname[0], SOS_FNAME_BUFSIZ);
	print_sword_filename(&swdname[0]);
	print_unix_filename(&swdname[0]);

	rc = read_fat_sword('A', SOS_FATPOS_DEFAULT, &rec[0]);
	sos_assert( rc == 0 );

	rc = write_fat_sword('A', SOS_FATPOS_DEFAULT, &rec[0]);
	sos_assert( rc == 0 );

	rc = read_fat_sword('A', SOS_FATPOS_DEFAULT, &rec2[0]);
	sos_assert( rc == 0 );

	rc = memcmp(rec,rec2,SOS_RECORD_SIZE);
	sos_assert( rc == 0 );

	rc = storage_record_read('A', &rec[0], SOS_DIRPS_DEFAULT, 1, &len);
	sos_assert( rc == 0 );
	sos_assert( len == 1 );

	rc = search_dent_sword('A', SOS_DIRPS_DEFAULT, "inside-r.obj", &fib);
	sos_assert( rc == SOS_ERROR_NOENT );
	rc = fs_unix2sword("cs84x1.bin", &swdname[0], SOS_FNAME_BUFSIZ);
	sos_assert( rc == 0 );

	rc = search_dent_sword('A', SOS_DIRPS_DEFAULT, &swdname[0], &fib);
	sos_assert( rc == 0 );

	return 0;
}
