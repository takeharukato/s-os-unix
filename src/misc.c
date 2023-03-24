/*
 *  SWORD Emurator
 *  Miscellaneous functions
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

/** Check whether the file exists and the file is readable
    @param[in] path  The file name to test.
    @param[in] flags open flags O_RDONLY, O_WRONLY or O_RDWR
    @retval  0  the file exists
    @retval  < 0 the file does not exist.
    @note This close the file descriptor, so it's not so secure.
 */
int
check_file_exists(const char *path, int flags){
	int fd;

	fd = open(path, flags);
	if ( 0 > fd )
		return -1;  /* Can not open */

	close(fd);
	return 0;
}

/** Convert ascii charachters which represents a number to an integer value.
 @param[in] numstr ascii charachters which represents a number
 @param[out] vp the converted integer number.
 @retval      0 success
 @retval ERANGE the value exceeded an integer range.
 @retval EINVAL the string contains an invalid character.
 */
int
ascii_to_int(const char *numstr, int *vp){
	long     v;

	errno = 0;
	v = strtol(numstr, NULL,10);
	if ( ( ( errno == ERANGE ) && ( ( v == LONG_MAX ) || ( v == LONG_MIN ) ) )
	    || ( ( errno != 0 ) && ( v == 0 ) ) )
		return errno;
	if ( ( v > INT_MAX ) || ( INT_MIN > v ) )
		return ERANGE;

	*vp = (int)v;

	return 0;
}
