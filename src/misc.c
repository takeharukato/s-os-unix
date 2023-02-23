/*
 *  SWORD Emurator
 *  Miscellaneous functions
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
