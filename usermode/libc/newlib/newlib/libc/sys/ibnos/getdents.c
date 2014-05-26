#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include "syscall.h"

int getdents(int fd, struct dirent *dirp, int count)
{
	int32_t ret;
	memset(dirp, 0, sizeof(*dirp));

	/* read name of object */
	ret = objectRead(fd, dirp->d_name, sizeof(dirp->d_name));
	if (ret < 0) return 0;

	dirp->d_ino		= 0;
	dirp->d_off		= sizeof(*dirp);
	dirp->d_reclen	= sizeof(*dirp);
	return sizeof(*dirp);
}